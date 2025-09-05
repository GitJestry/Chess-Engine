#include "lilia/engine/search.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <exception>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "lilia/engine/config.hpp"
#include "lilia/engine/move_buffer.hpp"
#include "lilia/engine/move_list.hpp"
#include "lilia/engine/move_order.hpp"
#include "lilia/engine/thread_pool.hpp"

namespace lilia::engine {

using steady_clock = std::chrono::steady_clock;

// ---------- kleine Helfer / Konstanten ----------

inline int mate_in(int ply) {
  return MATE - ply;
}
inline int mated_in(int ply) {
  return -MATE + ply;
}
inline bool is_mate_score(int s) {
  return std::abs(s) >= MATE_THR;
}
inline int cap_ply(int ply) {
  return ply < 0 ? 0 : (ply >= MAX_PLY ? (MAX_PLY - 1) : ply);
}

inline int encode_tt_score(int s, int ply) {
  if (s >= MATE_THR) return s + ply;
  if (s <= -MATE_THR) return s - ply;
  return s;
}
inline int decode_tt_score(int s, int ply) {
  if (s >= MATE_THR) return s - ply;
  if (s <= -MATE_THR) return s + ply;
  return s;
}
static constexpr int FUT_MARGIN[4] = {0, 110, 200, 280};

namespace {

// ---- Guards ----

struct MoveUndoGuard {
  model::Position& pos;
  bool applied = false;
  explicit MoveUndoGuard(model::Position& p) : pos(p) {}
  bool doMove(const model::Move& m) {
    applied = pos.doMove(m);
    return applied;
  }
  void rollback() {
    if (applied) {
      pos.undoMove();
      applied = false;
    }
  }
  ~MoveUndoGuard() {
    if (applied) pos.undoMove();
  }
};

struct NullUndoGuard {
  model::Position& pos;
  bool applied = false;
  explicit NullUndoGuard(model::Position& p) : pos(p) {}
  bool doNull() {
    applied = pos.doNullMove();
    return applied;
  }
  void rollback() {
    if (applied) {
      pos.undoNullMove();
      applied = false;
    }
  }
  ~NullUndoGuard() {
    if (applied) pos.undoNullMove();
  }
};

struct SearchStoppedException : public std::exception {
  const char* what() const noexcept override { return "Search stopped"; }
};

inline void check_stop(const std::shared_ptr<std::atomic<bool>>& stopFlag) {
  if (stopFlag && stopFlag->load()) throw SearchStoppedException();
}

// piece index [0..5] for Pawn..King
inline int pidx(core::PieceType pt) {
  using PT = core::PieceType;
  switch (pt) {
    case PT::Pawn:
      return 0;
    case PT::Knight:
      return 1;
    case PT::Bishop:
      return 2;
    case PT::Rook:
      return 3;
    case PT::Queen:
      return 4;
    case PT::King:
      return 5;
    default:
      return 0;
  }
}

// Schnelles MVV/LVA (SEE-frei) fürs Ordering

// --- kleine Int-Tools / History-Update ---

static inline int ilog2_u32(unsigned v) {
#if defined(__GNUC__) || defined(__clang__)
  return v ? 31 - __builtin_clz(v) : 0;
#else
  int r = 0;
  while (v >>= 1) ++r;
  return r;
#endif
}
static inline int iabs_int(int x) {
  return x < 0 ? -x : x;
}

static inline int hist_bonus(int depth) {
  unsigned x = (unsigned)(depth * depth) + 1u;
  int lg = ilog2_u32(x);
  return 16 + 8 * lg;  // 16,24,32,40,...
}

template <typename T>
static inline void hist_update(T& h, int bonus) {
  int x = (int)h;
  x += bonus - (x * iabs_int(bonus)) / 32768;
  if (x > 32767) x = 32767;
  if (x < -32768) x = -32768;
  h = (T)x;
}

static inline int gen_all(model::MoveGenerator& mg, model::Position& pos, model::Move* out,
                          int cap) {
  engine::MoveBuffer buf(out, cap);
  return mg.generatePseudoLegalMoves(pos.getBoard(), pos.getState(), buf);
}
static inline int gen_caps(model::MoveGenerator& mg, model::Position& pos, model::Move* out,
                           int cap) {
  engine::MoveBuffer buf(out, cap);
  return mg.generateCapturesOnly(pos.getBoard(), pos.getState(), buf);
}
static inline int gen_evasions(model::MoveGenerator& mg, model::Position& pos, model::Move* out,
                               int cap) {
  engine::MoveBuffer buf(out, cap);
  return mg.generateEvasions(pos.getBoard(), pos.getState(), buf);
}

// 0 = no immediate pawn attack; 1 = threatens Q/R/B/N; 2 = gives check
static inline int quiet_pawn_push_signal(const model::Board& b, const model::Move& m,
                                         core::Color us) {
  using PT = core::PieceType;
  if (m.isCapture() || m.promotion() != PT::None) return 0;

  auto mover = b.getPiece(m.from());
  if (!mover || mover->type != PT::Pawn) return 0;

  const auto toBB = model::bb::sq_bb(m.to());
  const auto atk = (us == core::Color::White) ? (model::bb::ne(toBB) | model::bb::nw(toBB))
                                              : (model::bb::se(toBB) | model::bb::sw(toBB));

  // Check first: pushing pawn gives check?
  if (atk & b.getPieces(~us, PT::King)) return 2;

  // Otherwise, immediate threat on heavy/minor pieces?
  const auto targets = b.getPieces(~us, PT::Queen) | b.getPieces(~us, PT::Rook) |
                       b.getPieces(~us, PT::Bishop) | b.getPieces(~us, PT::Knight);
  return (atk & targets) ? 1 : 0;
}

}  // namespace

// ---------- Search ----------

Search::Search(model::TT5& tt_, std::shared_ptr<const Evaluator> eval_, const EngineConfig& cfg_)
    : tt(tt_), mg(), cfg(cfg_), eval_(std::move(eval_)) {
  for (auto& kk : killers) {
    kk[0] = model::Move{};
    kk[1] = model::Move{};
  }
  for (auto& h : history) h.fill(0);
  std::fill(&quietHist[0][0], &quietHist[0][0] + PIECE_NB * SQ_NB, 0);
  std::fill(&captureHist[0][0][0], &captureHist[0][0][0] + PIECE_NB * SQ_NB * PIECE_NB, 0);
  std::fill(&counterHist[0][0], &counterHist[0][0] + SQ_NB * SQ_NB, 0);
  for (auto& row : counterMove)
    for (auto& m : row) m = model::Move{};
  for (auto& pm : prevMove) pm = model::Move{};
  std::fill(&contHist[0][0][0], &contHist[0][0][0] + PIECE_NB * SQ_NB * SQ_NB, 0);

  stopFlag.reset();
  sharedNodes.reset();  // NEW
  nodeLimit = 0;        // NEW
  stats = SearchStats{};
}

int Search::signed_eval(model::Position& pos) {
  int v = eval_->evaluate(pos);
  if (pos.getState().sideToMove == core::Color::Black) v = -v;
  return std::clamp(v, -MATE + 1, MATE - 1);
}

inline void bump_node_or_stop(const std::shared_ptr<std::atomic<std::uint64_t>>& counter,
                              std::uint64_t limit,
                              const std::shared_ptr<std::atomic<bool>>& stopFlag) {
  if (!counter) return;
  // old code: std::uint64_t prev = counter->fetch_add(1, std::memory_order_relaxed);
  const std::uint64_t cur = counter->fetch_add(1, std::memory_order_relaxed) + 1;
  if (limit && cur >= limit) {
    if (stopFlag) stopFlag->store(true, std::memory_order_relaxed);
    throw SearchStoppedException();
  }
}

// ---------- Quiescence + QTT ----------

int Search::quiescence(model::Position& pos, int alpha, int beta, int ply) {
  stats.nodes++;
  bump_node_or_stop(sharedNodes, nodeLimit, stopFlag);
  check_stop(stopFlag);

  if (ply >= MAX_PLY - 2) return signed_eval(pos);

  const int kply = cap_ply(ply);
  const uint64_t parentKey = pos.hash();
  const int alphaOrig = alpha, betaOrig = beta;

  // QTT probe (depth == 0)
  {
    model::TTEntry5 tte{};
    if (tt.probe_into(pos.hash(), tte)) {
      const int ttVal = decode_tt_score(tte.value, kply);
      if (tte.depth == 0) {
        if (tte.bound == model::Bound::Exact) return ttVal;
        if (tte.bound == model::Bound::Lower && ttVal >= beta) return ttVal;
        if (tte.bound == model::Bound::Upper && ttVal <= alpha) return ttVal;
      }
    }
  }
  const bool inCheck = (ply == 0) ? pos.inCheck() : pos.lastMoveGaveCheck();
  if (inCheck) {
    // nur Evasions generieren
    int n = gen_evasions(mg, pos, genArr_[kply], engine::MAX_MOVES);
    if (n <= 0) return mated_in(ply);

    constexpr int MAXM = engine::MAX_MOVES;
    int scores[MAXM];
    model::Move ordered[MAXM];

    const model::Move prev = (ply > 0 ? prevMove[cap_ply(ply - 1)] : model::Move{});
    const bool prevOk = (prev.from() >= 0 && prev.to() >= 0 && prev.from() < 64 && prev.to() < 64);
    const model::Move cm = prevOk ? counterMove[prev.from()][prev.to()] : model::Move{};

    for (int i = 0; i < n; ++i) {
      const auto& m = genArr_[kply][i];
      int s = 0;
      if (prevOk && m == cm) s += 80'000;
      if (m.isCapture()) s += 100'000 + mvv_lva_fast(pos, m);
      if (m.promotion() != core::PieceType::None) s += 60'000;
      s += history[m.from()][m.to()];
      scores[i] = s;
      ordered[i] = m;
    }
    sort_by_score_desc(scores, ordered, n);

    int best = -INF;
    bool anyLegal = false;

    for (int i = 0; i < n; ++i) {
      check_stop(stopFlag);
      const model::Move m = ordered[i];

      MoveUndoGuard g(pos);
      if (!g.doMove(m)) continue;
      anyLegal = true;

      prevMove[cap_ply(ply)] = m;
      tt.prefetch(pos.hash());
      int score = -quiescence(pos, -beta, -alpha, ply + 1);
      score = std::clamp(score, -MATE + 1, MATE - 1);

      if (score >= beta) {
        if (!(stopFlag && stopFlag->load()))
          tt.store(parentKey, encode_tt_score(beta, kply), 0, model::Bound::Lower, model::Move{});
        return beta;
      }
      if (score > best) best = score;
      if (score > alpha) alpha = score;
    }

    if (!anyLegal) {
      const int ms = mated_in(ply);
      if (!(stopFlag && stopFlag->load()))
        tt.store(parentKey, encode_tt_score(ms, kply), 0, model::Bound::Exact, model::Move{});
      return ms;
    }

    if (!(stopFlag && stopFlag->load())) {
      model::Bound b = model::Bound::Exact;
      if (best <= alphaOrig)
        b = model::Bound::Upper;
      else if (best >= betaOrig)
        b = model::Bound::Lower;
      tt.store(parentKey, encode_tt_score(best, kply), 0, b, model::Move{});
    }
    return best;
  }

  int stand = signed_eval(pos);
  if (stand >= beta) {
    if (!(stopFlag && stopFlag->load()))
      tt.store(parentKey, encode_tt_score(beta, kply), 0, model::Bound::Lower, model::Move{});
    return beta;
  }
  if (alpha < stand) alpha = stand;

  // nur Captures generieren
  int qn = gen_caps(mg, pos, capArr_[kply], engine::MAX_MOVES);
  if (qn <= 0) {
    if (!(stopFlag && stopFlag->load()))
      tt.store(parentKey, encode_tt_score(stand, kply), 0, model::Bound::Exact, model::Move{});
    return stand;
  }

  // Captures sortieren via precomputed MVV-LVA Scores (keine std::sort mit Lambda im Hotpath)
  constexpr int MAXM = engine::MAX_MOVES;
  int qs[MAXM];
  model::Move qord[MAXM];
  for (int i = 0; i < qn; ++i) {
    const auto& m = capArr_[kply][i];
    qs[i] = mvv_lva_fast(pos, m);
    qord[i] = m;
  }
  sort_by_score_desc(qs, qord, qn);

  constexpr int DELTA_MARGIN = 96;
  int best = stand;

  for (int i = 0; i < qn; ++i) {
    const model::Move m = qord[i];
    check_stop(stopFlag);

    if (m.isCapture() && !pos.see(m) && m.promotion() == core::PieceType::None) continue;

    const bool isQuietPromo = (m.promotion() != core::PieceType::None) && !m.isCapture();

    int capVal = 0;
    if (m.isEnPassant())
      capVal = base_value[(int)core::PieceType::Pawn];
    else if (m.isCapture()) {
      if (auto cap = pos.getBoard().getPiece(m.to())) capVal = base_value[(int)cap->type];
    }

    int promoGain = 0;
    if (m.promotion() != core::PieceType::None)
      promoGain =
          std::max(0, base_value[(int)m.promotion()] - base_value[(int)core::PieceType::Pawn]);

    if (!isQuietPromo) {
      if (stand + capVal + promoGain + DELTA_MARGIN <= alpha) continue;  // Delta
    }

    MoveUndoGuard g(pos);
    if (!g.doMove(m)) continue;

    prevMove[cap_ply(ply)] = m;
    tt.prefetch(pos.hash());
    int score = -quiescence(pos, -beta, -alpha, ply + 1);
    score = std::clamp(score, -MATE + 1, MATE - 1);

    if (score >= beta) {
      if (!(stopFlag && stopFlag->load()))
        tt.store(parentKey, encode_tt_score(beta, kply), 0, model::Bound::Lower, model::Move{});
      return beta;
    }
    if (score > alpha) alpha = score;
    if (score > best) best = score;
  }

  if (!(stopFlag && stopFlag->load())) {
    model::Bound b = model::Bound::Exact;
    if (best <= alphaOrig)
      b = model::Bound::Upper;
    else if (best >= betaOrig)
      b = model::Bound::Lower;
    tt.store(parentKey, encode_tt_score(best, kply), 0, b, model::Move{});
  }
  return best;
}

// ---------- Negamax ----------

int Search::negamax(model::Position& pos, int depth, int alpha, int beta, int ply,
                    model::Move& refBest, int parentStaticEval) {
  stats.nodes++;
  bump_node_or_stop(sharedNodes, nodeLimit, stopFlag);
  check_stop(stopFlag);

  if (ply >= MAX_PLY - 2) return signed_eval(pos);
  if (pos.checkInsufficientMaterial() || pos.checkMoveRule() || pos.checkRepetition()) return 0;
  if (depth <= 0) return quiescence(pos, alpha, beta, ply);

  // Mate distance pruning
  alpha = std::max(alpha, mated_in(ply));
  beta = std::min(beta, mate_in(ply));
  if (alpha >= beta) return alpha;

  const int origAlpha = alpha;
  const int origBeta = beta;
  const bool isPV = (beta - alpha > 1);

  const bool inCheck = (ply == 0) ? pos.inCheck() : pos.lastMoveGaveCheck();
  const int staticEval = inCheck ? 0 : signed_eval(pos);

  // SNMP
  if (!inCheck && !isPV && depth <= 3) {
    const int nonP = [&] {
      using PT = core::PieceType;
      auto countSide = [&](core::Color c) {
        return model::bb::popcount(
            pos.getBoard().getPieces(c, PT::Knight) | pos.getBoard().getPieces(c, PT::Bishop) |
            pos.getBoard().getPieces(c, PT::Rook) | pos.getBoard().getPieces(c, PT::Queen));
      };
      return countSide(core::Color::White) + countSide(core::Color::Black);
    }();
    if (nonP >= 6) {
      static constexpr int margins[4] = {0, 140, 200, 260};
      if (staticEval - margins[depth] >= beta) return staticEval;
    }
  }

  // Razoring D1
  if (!inCheck && !isPV && depth == 1) {
    const int razorMargin = 220;
    if (staticEval + razorMargin <= alpha) {
      int q = quiescence(pos, alpha - 1, alpha, ply);
      if (q <= alpha) return q;
    }
  }

  // Reverse futility D1
  if (!inCheck && !isPV && depth == 1) {
    const int margin = 180;
    if (staticEval - margin >= beta) return staticEval;
  }

  int best = -INF;
  model::Move bestLocal{};

  // TT
  model::Move ttMove{};
  bool haveTT = false;
  int ttVal = 0;
  if (model::TTEntry5 tte{}; tt.probe_into(pos.hash(), tte)) {
    haveTT = true;
    ttMove = tte.best;
    ttVal = decode_tt_score(tte.value, cap_ply(ply));
    if (tte.depth >= depth) {
      if (tte.bound == model::Bound::Exact) return std::clamp(ttVal, -MATE + 1, MATE - 1);
      if (tte.bound == model::Bound::Lower) alpha = std::max(alpha, ttVal);
      if (tte.bound == model::Bound::Upper) beta = std::min(beta, ttVal);
      if (alpha >= beta) return std::clamp(ttVal, -MATE + 1, MATE - 1);
    }
  }

  // Null move pruning
  auto nonPawnCount = [&](const model::Board& b) {
    using PT = core::PieceType;
    auto countSide = [&](core::Color c) {
      return model::bb::popcount(b.getPieces(c, PT::Knight) | b.getPieces(c, PT::Bishop) |
                                 b.getPieces(c, PT::Rook) | b.getPieces(c, PT::Queen));
    };
    return countSide(core::Color::White) + countSide(core::Color::Black);
  };
  const bool sparse = (nonPawnCount(pos.getBoard()) <= 3);
  const bool prevWasCapture = (ply > 0 && prevMove[cap_ply(ply - 1)].isCapture());

  if (cfg.useNullMove && depth >= 3 && !inCheck && !isPV && !sparse && !prevWasCapture) {
    const int margin = 50 + 20 * depth;
    if (staticEval >= beta + margin) {
      NullUndoGuard ng(pos);
      if (ng.doNull()) {
        int R = (depth >= 7 ? 3 : 2);
        int nullScore = -negamax(pos, depth - 1 - R, -beta, -beta + 1, ply + 1, refBest);
        if (nullScore >= beta) {
          if (depth >= 6) {
            int verify = -negamax(pos, depth - 1, -beta, -beta + 1, ply + 1, refBest);
            if (verify >= beta) return beta;
          } else
            return beta;
        }
      }
    }
  }

  // Generierung
  const int kply = cap_ply(ply);
  int n = 0;
  if (inCheck) {
    n = gen_evasions(mg, pos, genArr_[kply], engine::MAX_MOVES);
    if (n <= 0) return mated_in(ply);
  } else {
    n = gen_all(mg, pos, genArr_[kply], engine::MAX_MOVES);
    if (n <= 0) return 0;
  }

  // prev für CounterMove
  const model::Move prev = (ply > 0 ? prevMove[cap_ply(ply - 1)] : model::Move{});
  const bool prevOk = (prev.from() >= 0 && prev.to() >= 0 && prev.from() < 64 && prev.to() < 64);
  const model::Move cm = prevOk ? counterMove[prev.from()][prev.to()] : model::Move{};

  // Ordering
  constexpr int MAX_MOVES = engine::MAX_MOVES;
  int scores[MAX_MOVES];
  model::Move ordered[MAX_MOVES];

  constexpr int TT_BONUS = 2'400'000;
  constexpr int CAP_BASE_GOOD = 180'000;
  constexpr int PROMO_BASE = 160'000;
  constexpr int KILLER_BASE = 120'000;
  constexpr int CM_BASE = 140'000;

  const auto& board = pos.getBoard();

  for (int i = 0; i < n; ++i) {
    const auto& m = genArr_[kply][i];
    int s = 0;

    if (haveTT && m == ttMove) {
      s = TT_BONUS;
    } else if (m.isCapture() || m.promotion() != core::PieceType::None) {
      auto moverOpt = board.getPiece(m.from());
      const core::PieceType moverPt = moverOpt ? moverOpt->type : core::PieceType::Pawn;
      core::PieceType capPt = core::PieceType::Pawn;
      if (m.isEnPassant())
        capPt = core::PieceType::Pawn;
      else if (auto cap = board.getPiece(m.to()))
        capPt = cap->type;

      const int mvv = mvv_lva_fast(pos, m);
      const int ch = captureHist[pidx(moverPt)][m.to()][pidx(capPt)];
      if (m.promotion() != core::PieceType::None && !m.isCapture())
        s = PROMO_BASE + mvv;
      else
        s = CAP_BASE_GOOD + mvv + (ch >> 2);
    } else {
      auto moverOpt = board.getPiece(m.from());
      const core::PieceType moverPt = moverOpt ? moverOpt->type : core::PieceType::Pawn;
      s = history[m.from()][m.to()] + (quietHist[pidx(moverPt)][m.to()] >> 1);
      if (m == killers[kply][0] || m == killers[kply][1]) s += KILLER_BASE;
      if (prevOk && m == cm) s += CM_BASE + (counterHist[prev.from()][prev.to()] >> 1);

      // NEW: tactical quiet pawn push bonus (use signal)
      const auto us = pos.getState().sideToMove;
      const int sig = quiet_pawn_push_signal(board, m, us);
      if (sig == 2)
        s += 220'000;  // pawn push gives check
      else if (sig == 1)
        s += 180'000;  // immediate threat
    }

    scores[i] = s;
    ordered[i] = m;
  }
  sort_by_score_desc(scores, ordered, n);

  const bool allowFutility = !inCheck && !isPV;
  int moveCount = 0;

  for (int idx = 0; idx < n; ++idx) {
    check_stop(stopFlag);

    const model::Move m = ordered[idx];
    const bool isQuiet = !m.isCapture() && (m.promotion() == core::PieceType::None);
    const auto us = pos.getState().sideToMove;
    const int qp_sig = isQuiet ? quiet_pawn_push_signal(board, m, us) : 0;
    const bool tacticalQuiet = (qp_sig > 0);  // 2 = check, 1 = threat

    // pre info
    auto moverOpt = board.getPiece(m.from());
    const core::PieceType moverPt = moverOpt ? moverOpt->type : core::PieceType::Pawn;
    core::PieceType capPt = core::PieceType::Pawn;
    if (m.isEnPassant())
      capPt = core::PieceType::Pawn;
    else if (m.isCapture()) {
      if (auto cap = board.getPiece(m.to())) capPt = cap->type;
    }
    const int capValPre = m.isCapture() ? (m.isEnPassant() ? base_value[(int)core::PieceType::Pawn]
                                                           : base_value[(int)capPt])
                                        : 0;

    // LMP
    if (!inCheck && !isPV && isQuiet && depth <= 3 && !tacticalQuiet) {
      int limit = depth * depth;  // 1,4,9
      int h = history[m.from()][m.to()] + (quietHist[pidx(moverPt)][m.to()] >> 1);
      if (h < -8000) limit = std::max(1, limit - 1);
      if (staticEval + FUT_MARGIN[depth] <= alpha + 32 && moveCount >= limit) {
        ++moveCount;
        continue;
      }
    }

    // Extended futility (depth<=3, quiets)
    if (allowFutility && isQuiet && depth <= 3 && !tacticalQuiet) {
      int fut = FUT_MARGIN[depth] + (history[m.from()][m.to()] < -8000 ? 32 : 0);
      if (staticEval + fut <= alpha) {
        ++moveCount;
        continue;
      }
    }

    // History pruning
    if (!inCheck && !isPV && isQuiet && depth <= 2 && !tacticalQuiet) {
      int histScore = history[m.from()][m.to()] + (quietHist[pidx(moverPt)][m.to()] >> 1);
      if (histScore < -11000 && m != killers[kply][0] && m != killers[kply][1] &&
          (!prevOk || m != cm)) {
        ++moveCount;
        continue;
      }
    }

    // Futility (D1)
    if (!inCheck && !isPV && isQuiet && depth == 1 && !tacticalQuiet) {
      if (staticEval + 110 <= alpha) {
        ++moveCount;
        continue;
      }
    }

    // SEE pruning (after futility)
    bool seeGood = true;
    if (!inCheck && ply > 0 && m.isCapture() && depth <= 5) {
      if (!pos.see(m)) {
        ++moveCount;
        continue;
      }
    } else if (m.isCapture()) {
      seeGood = pos.see(m);
    }

    const int mvvBefore =
        (m.isCapture() || m.promotion() != core::PieceType::None) ? mvv_lva_fast(pos, m) : 0;

    MoveUndoGuard g(pos);
    if (!g.doMove(m)) {
      ++moveCount;
      continue;
    }

    prevMove[cap_ply(ply)] = m;
    tt.prefetch(pos.hash());

    int value;
    model::Move childBest{};
    int newDepth = depth - 1;

    // ProbCut (light) — use pre-move captured value
    if (!isPV && !inCheck && depth >= 5 && m.isCapture() && seeGood && mvvBefore >= 700) {
      constexpr int PROBCUT_MARGIN = 180;
      if (staticEval + capValPre + PROBCUT_MARGIN >= beta) {
        const int red = 3;
        const int probe = -negamax(pos, depth - red, -beta, -(beta - 1), ply + 1, childBest);
        if (probe >= beta) return beta;
      }
    }

    // Check extension (light)
    const bool givesCheck = pos.lastMoveGaveCheck();
    if (givesCheck && (isQuiet || seeGood)) newDepth += 1;

    // Bad capture reduction
    int reduction = 0;
    if (!seeGood && m.isCapture() && newDepth >= 2) reduction = std::min(1, newDepth - 1);

    // PVS / LMR
    if (moveCount == 0) {
      value = -negamax(pos, newDepth, -beta, -alpha, ply + 1, childBest);
    } else {
      if (cfg.useLMR && isQuiet && !tacticalQuiet && !inCheck && !givesCheck && newDepth >= 2 &&
          moveCount >= 3) {
        const int ld = ilog2_u32((unsigned)depth);
        const int lm = ilog2_u32((unsigned)(moveCount + 1));
        int rBase = (ld * (lm + 1)) / 3;

        const int h = history[m.from()][m.to()] + (quietHist[pidx(moverPt)][m.to()] >> 1);
        if (h > 8000) rBase -= 1;
        if (h < -8000) rBase += 1;

        if (m == killers[kply][0] || m == killers[kply][1]) rBase -= 1;
        if (haveTT && m == ttMove) rBase -= 1;

        if (ply <= 2) rBase -= 1;
        if (beta - alpha <= 8) rBase -= 1;

        if (rBase < 0) rBase = 0;
        int rCap = (newDepth >= 5 ? 3 : 2);
        if (rBase > rCap) rBase = rCap;
        reduction = std::min(rBase, newDepth - 1);
      }

      value = -negamax(pos, newDepth - reduction, -alpha - 1, -alpha, ply + 1, childBest);
      if (value > alpha && value < beta) {
        value = -negamax(pos, newDepth, -beta, -alpha, ply + 1, childBest);
      }
    }

    value = std::clamp(value, -MATE + 1, MATE - 1);

    // History updates
    if (isQuiet && value <= origAlpha) {
      hist_update(history[m.from()][m.to()], -hist_bonus(depth) / 2);
      hist_update(quietHist[pidx(moverPt)][m.to()], -hist_bonus(depth) / 2);
    }

    if (value > best) {
      best = value;
      bestLocal = m;
    }
    if (value > alpha) alpha = value;

    if (alpha >= beta) {
      if (isQuiet) {
        killers[kply][1] = killers[kply][0];
        killers[kply][0] = m;
        hist_update(history[m.from()][m.to()], +hist_bonus(depth));
        hist_update(quietHist[pidx(moverPt)][m.to()], +hist_bonus(depth));
        if (prevOk) {
          counterMove[prev.from()][prev.to()] = m;
          hist_update(counterHist[prev.from()][prev.to()], +hist_bonus(depth));
        }
      } else {
        hist_update(captureHist[pidx(moverPt)][m.to()][pidx(capPt)], +hist_bonus(depth));
      }
      break;
    }
    ++moveCount;
  }

  if (best == -INF) {
    if (inCheck) return mated_in(ply);
    return 0;
  }

  if (!(stopFlag && stopFlag->load())) {
    model::Bound b;
    if (best <= origAlpha)
      b = model::Bound::Upper;
    else if (best >= origBeta)
      b = model::Bound::Lower;
    else
      b = model::Bound::Exact;
    tt.store(pos.hash(), encode_tt_score(best, cap_ply(ply)), static_cast<int16_t>(depth), b,
             bestLocal);
  }

  refBest = bestLocal;
  return best;
}

// ---------- PV aus TT ----------

std::vector<model::Move> Search::build_pv_from_tt(model::Position pos, int max_len) {
  std::vector<model::Move> pv;
  std::unordered_set<uint64_t> seen;
  pv.reserve(max_len);
  seen.reserve(max_len);
  for (int i = 0; i < max_len; ++i) {
    model::TTEntry5 tte{};
    if (!tt.probe_into(pos.hash(), tte)) break;
    model::Move m = tte.best;

    if (m.from() == m.to()) break;
    if (!pos.doMove(m)) break;
    pv.push_back(m);

    uint64_t h = pos.hash();
    if (!seen.insert(h).second) break;  // TT-Loop erkannt
  }
  return pv;
}

// ---------- Root Search (parallel, Work-Queue, fixes) ----------

int Search::search_root_parallel(model::Position& pos, int maxDepth,
                                 std::shared_ptr<std::atomic<bool>> stop, int maxThreads,
                                 std::uint64_t maxNodes /* = 0 */) {
  this->stopFlag = stop;
  stats = SearchStats{};

  // Shared node counter for optional node cap (0 = unlimited)
  auto sharedNodeCounter = std::make_shared<std::atomic<std::uint64_t>>(0);
  this->sharedNodes = sharedNodeCounter;
  this->nodeLimit = maxNodes;

  auto t0 = steady_clock::now();
  auto update_time_stats = [&] {
    auto now = steady_clock::now();
    std::uint64_t ms =
        (std::uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
    stats.elapsedMs = ms;
    stats.nps = (ms > 0) ? (double)stats.nodes / (ms / 1000.0) : (double)stats.nodes;
  };

  try {
    tt.new_generation();
  } catch (...) {
  }

  // Root-Moves
  std::vector<model::Move> rootMoves;
  mg.generatePseudoLegalMoves(pos.getBoard(), pos.getState(), rootMoves);
  if (rootMoves.empty()) {
    update_time_stats();
    this->stopFlag.reset();
    return 0;
  }

  // Thread-Pool setup (pool initialized at engine startup)
  auto& pool = ThreadPool::instance();
  const int threads =
      std::max(1, maxThreads > 0 ? std::min(maxThreads, cfg.threads) : cfg.threads);

  // Aspiration seed
  int lastScoreGuess = 0;
  if (cfg.useAspiration) {
    model::TTEntry5 tte{};
    if (tt.probe_into(pos.hash(), tte)) lastScoreGuess = decode_tt_score(tte.value, 0);
  }

  auto score_root_move = [&](const model::Move& m, const model::Move& ttMove, bool haveTT) {
    int s = 0;
    if (haveTT && m == ttMove) s += 2'500'000;
    if (m.isCapture()) s += 1'100'000 + mvv_lva_fast(pos, m);
    if (m.promotion() != core::PieceType::None) s += 1'050'000;
    // NEW: tactical quiet pawn pushes at root
    if (!m.isCapture() && m.promotion() == core::PieceType::None) {
      const int sig = quiet_pawn_push_signal(pos.getBoard(), m, pos.getState().sideToMove);
      if (sig == 2)
        s += 220'000;
      else if (sig == 1)
        s += 180'000;
    }
    s += history[m.from()][m.to()];
    return s;
  };

  struct RootResult {
    std::atomic<int> nullScore{std::numeric_limits<int>::min()};
    std::atomic<int> fullScore{std::numeric_limits<int>::min()};
  };

  for (int depth = 1; depth <= std::max(1, maxDepth); ++depth) {
    if (stop && stop->load()) break;

    // Re-Order Root-Moves
    model::Move rootTT{};
    bool haveRootTT = false;
    if (model::TTEntry5 tte{}; tt.probe_into(pos.hash(), tte)) {
      haveRootTT = true;
      rootTT = tte.best;
    }

    std::sort(rootMoves.begin(), rootMoves.end(), [&](const model::Move& a, const model::Move& b) {
      int sa = score_root_move(a, rootTT, haveRootTT);
      int sb = score_root_move(b, rootTT, haveRootTT);
      if (sa != sb) return sa > sb;
      if (a.from() != b.from()) return a.from() < b.from();
      return a.to() < b.to();
    });

    // Ergebnis-Container pro Move
    std::vector<RootResult> results(rootMoves.size());

    // Shared alpha
    std::atomic<int> sharedAlpha{-INF};

    // 1) Erstes Kind synchron mit Aspiration
    {
      const model::Move m0 = rootMoves.front();
      model::Position child = pos;
      if (child.doMove(m0)) {
        tt.prefetch(child.hash());
        Search worker(tt, eval_, cfg);
        worker.stopFlag = stop;
        worker.set_node_limit(sharedNodeCounter, maxNodes);  // node cap
        worker.prevMove[0] = m0;

        model::Move ref{};
        int s = 0;
        try {
          if (!cfg.useAspiration || depth - 1 < 3 || is_mate_score(lastScoreGuess)) {
            s = -worker.negamax(child, depth - 1, -INF, INF, 1, ref);
          } else {
            int w = std::max(12, cfg.aspirationWindow);
            int low = lastScoreGuess - w, high = lastScoreGuess + w;
            for (int tries = 0; tries < 3; ++tries) {
              s = -worker.negamax(child, depth - 1, -high, -low, 1, ref);
              s = std::clamp(s, -MATE + 1, MATE - 1);
              if (!is_mate_score(s) && s > low && s < high) break;
              const int step = 64 + 16 * tries;
              if (s <= low)
                low -= step;
              else if (s >= high)
                high += step;
              else
                break;
              if (is_mate_score(s)) break;
            }
            if (!(s > (lastScoreGuess - std::max(12, cfg.aspirationWindow)) &&
                  s < (lastScoreGuess + std::max(12, cfg.aspirationWindow)))) {
              s = -worker.negamax(child, depth - 1, -INF, INF, 1, ref);
            }
          }
        } catch (const SearchStoppedException&) {
          if (stop) stop->store(true, std::memory_order_relaxed);
          // sync reported nodes with the shared counter before returning
          stats.nodes = sharedNodeCounter->load(std::memory_order_relaxed);
          update_time_stats();
          this->stopFlag.reset();
          return stats.bestScore;
        }

        s = std::clamp(s, -MATE + 1, MATE - 1);
        results[0].nullScore.store(s, std::memory_order_relaxed);
        results[0].fullScore.store(s, std::memory_order_relaxed);
        sharedAlpha.store(s, std::memory_order_relaxed);
        stats.nodes += worker.getStats().nodes;
      }
    }

    // 2) Restliche Root-Moves per Work-Queue
    const int total = (int)rootMoves.size();
    std::atomic<int> idx{1};

    std::atomic<uint64_t> nodesAcc{0};

    auto workerFn = [&](int /*threadId*/) {
      Search worker(tt, eval_, cfg);
      worker.stopFlag = stop;
      worker.set_node_limit(sharedNodeCounter, maxNodes);  // node cap
      try {
        for (;;) {
          if (stop && stop->load()) break;
          int i = idx.fetch_add(1, std::memory_order_relaxed);
          if (i >= total) break;

          const model::Move m = rootMoves[i];
          model::Position child = pos;
          if (!child.doMove(m)) continue;

          tt.prefetch(child.hash());
          worker.prevMove[0] = m;
          model::Move ref{};

          int localAlpha = sharedAlpha.load(std::memory_order_relaxed);

          uint64_t before = worker.getStats().nodes;
          int sN = -worker.negamax(child, depth - 1, -(localAlpha + 1), -localAlpha, 1, ref);
          sN = std::clamp(sN, -MATE + 1, MATE - 1);
          nodesAcc.fetch_add(worker.getStats().nodes - before, std::memory_order_relaxed);

          results[i].nullScore.store(sN, std::memory_order_relaxed);

          if (sN >= localAlpha && !(stop && stop->load())) {
            before = worker.getStats().nodes;
            int sF = -worker.negamax(child, depth - 1, -INF, INF, 1, ref);
            sF = std::clamp(sF, -MATE + 1, MATE - 1);
            nodesAcc.fetch_add(worker.getStats().nodes - before, std::memory_order_relaxed);

            results[i].fullScore.store(sF, std::memory_order_relaxed);

            int cur = localAlpha;
            while (sF > cur &&
                   !sharedAlpha.compare_exchange_weak(cur, sF, std::memory_order_relaxed)) {
            }
          }
        }
      } catch (const SearchStoppedException&) {
        if (stop) stop->store(true, std::memory_order_relaxed);
        // thread exits quietly
      }
    };

    const int workers = std::min(total - 1, threads - 1);
    std::vector<std::future<void>> futs;
    futs.reserve(workers);
    for (int t = 0; t < workers; ++t) {
      futs.emplace_back(pool.submit([&, tid = t] { workerFn(tid); }));
    }
    for (auto& f : futs) {
      try {
        f.get();
      } catch (...) {
      }
    }
    stats.nodes += nodesAcc.load(std::memory_order_relaxed);

    // --- NEW: sequential fallback so 1-thread works and to mop up any leftovers ---
    for (int i = 1; i < total; ++i) {
      if (stop && stop->load()) break;
      if (results[i].nullScore.load(std::memory_order_relaxed) != std::numeric_limits<int>::min())
        continue;  // already processed by a worker

      model::Position child = pos;
      if (!child.doMove(rootMoves[i])) continue;

      Search worker(tt, eval_, cfg);
      worker.stopFlag = stop;
      worker.set_node_limit(sharedNodeCounter, maxNodes);  // node cap
      worker.prevMove[0] = rootMoves[i];

      model::Move ref{};
      try {
        int localAlpha = sharedAlpha.load(std::memory_order_relaxed);

        std::uint64_t before = worker.getStats().nodes;
        int sN = -worker.negamax(child, depth - 1, -(localAlpha + 1), -localAlpha, 1, ref);
        sN = std::clamp(sN, -MATE + 1, MATE - 1);
        results[i].nullScore.store(sN, std::memory_order_relaxed);
        stats.nodes += worker.getStats().nodes - before;

        if (sN >= localAlpha && !(stop && stop->load())) {
          before = worker.getStats().nodes;
          int sF = -worker.negamax(child, depth - 1, -INF, INF, 1, ref);
          sF = std::clamp(sF, -MATE + 1, MATE - 1);
          results[i].fullScore.store(sF, std::memory_order_relaxed);
          stats.nodes += worker.getStats().nodes - before;

          int cur = localAlpha;
          while (sF > cur &&
                 !sharedAlpha.compare_exchange_weak(cur, sF, std::memory_order_relaxed)) {
          }
        }
      } catch (const SearchStoppedException&) {
        if (stop) stop->store(true, std::memory_order_relaxed);
        break;  // leave fallback loop
      }
    }

    // 3) Confirm-Pass (guarded)
    try {
      const int bestAlphaNow = sharedAlpha.load(std::memory_order_relaxed);
      for (int i = 1; i < total; ++i) {
        if (stop && stop->load()) break;
        const int sN = results[i].nullScore.load(std::memory_order_relaxed);
        const int sF = results[i].fullScore.load(std::memory_order_relaxed);
        if (sF != std::numeric_limits<int>::min()) continue;
        if (sN < bestAlphaNow) continue;

        model::Position child = pos;
        if (!child.doMove(rootMoves[i])) continue;

        Search worker(tt, eval_, cfg);
        worker.stopFlag = stop;
        worker.set_node_limit(sharedNodeCounter, maxNodes);  // node cap
        worker.prevMove[0] = rootMoves[i];

        model::Move ref{};
        std::uint64_t before = worker.getStats().nodes;
        int sF2 = -worker.negamax(child, depth - 1, -INF, INF, 1, ref);
        sF2 = std::clamp(sF2, -MATE + 1, MATE - 1);
        results[i].fullScore.store(sF2, std::memory_order_relaxed);
        stats.nodes += worker.getStats().nodes - before;

        int cur = sharedAlpha.load(std::memory_order_relaxed);
        while (sF2 > cur &&
               !sharedAlpha.compare_exchange_weak(cur, sF2, std::memory_order_relaxed)) {
        }
      }
    } catch (const SearchStoppedException&) {
      if (stop) stop->store(true, std::memory_order_relaxed);
      // fall through to ranking with what we have
    }

    // 4) Ranking
    // If we were interrupted this iteration, keep the previous depth's result.
    if (stop && stop->load()) {
      break;  // exits the depth loop without touching stats
    }

    std::vector<std::pair<int, model::Move>> depthCand;
    depthCand.reserve(total);
    for (int i = 0; i < total; ++i) {
      int sF = results[i].fullScore.load(std::memory_order_relaxed);
      int sN = results[i].nullScore.load(std::memory_order_relaxed);
      int s = (sF != std::numeric_limits<int>::min()) ? sF : sN;
      depthCand.emplace_back(s, rootMoves[i]);
    }

    // If nothing was searched (e.g., immediate stop), keep last stats and bail.
    bool anyScored = std::any_of(depthCand.begin(), depthCand.end(), [](const auto& p) {
      return p.first != std::numeric_limits<int>::min();
    });
    if (!anyScored) break;

    int NDISPLAY = (depth >= 6 ? 2 : 1);
    NDISPLAY = std::min<int>(NDISPLAY, (int)depthCand.size());
    std::partial_sort(depthCand.begin(), depthCand.begin() + std::max(1, NDISPLAY), depthCand.end(),
                      [](const auto& a, const auto& b) { return a.first > b.first; });

    update_time_stats();

    const int bestScore = depthCand.front().first;
    const model::Move bestMove = depthCand.front().second;

    stats.bestScore = std::clamp(bestScore, -MATE + 1, MATE - 1);
    stats.bestMove = bestMove;

    stats.bestPV.clear();
    {
      model::Position tmp = pos;
      if (tmp.doMove(bestMove)) {
        stats.bestPV.push_back(bestMove);
        auto rest = build_pv_from_tt(tmp, 32);
        for (auto& mv : rest) stats.bestPV.push_back(mv);
      }
    }

    // After building depthCand
    const int SHOW = std::min<int>(5, (int)depthCand.size());

    // Ensure we actually have the best SHOW moves at the front
    std::partial_sort(depthCand.begin(), depthCand.begin() + SHOW, depthCand.end(),
                      [](const auto& a, const auto& b) { return a.first > b.first; });

    stats.topMoves.clear();
    for (int i = 0; i < SHOW; ++i) {
      stats.topMoves.push_back(
          {depthCand[i].second, std::clamp(depthCand[i].first, -MATE + 1, MATE - 1)});
    }
    if (is_mate_score(stats.bestScore)) break;
    lastScoreGuess = bestScore;
  }

  stats.nodes = sharedNodeCounter->load(std::memory_order_relaxed);
  this->stopFlag.reset();
  update_time_stats();
  return stats.bestScore;
}

void Search::clearSearchState() {
  for (auto& kk : killers) {
    kk[0] = model::Move{};
    kk[1] = model::Move{};
  }
  for (auto& h : history) h.fill(0);
  std::fill(&quietHist[0][0], &quietHist[0][0] + PIECE_NB * SQ_NB, 0);
  std::fill(&captureHist[0][0][0], &captureHist[0][0][0] + PIECE_NB * SQ_NB * PIECE_NB, 0);
  std::fill(&counterHist[0][0], &counterHist[0][0] + SQ_NB * SQ_NB, 0);
  for (auto& row : counterMove)
    for (auto& m : row) m = model::Move{};
  for (auto& pm : prevMove) pm = model::Move{};
  std::fill(&contHist[0][0][0], &contHist[0][0][0] + PIECE_NB * SQ_NB * SQ_NB, 0);
  stats = SearchStats{};
}

}  // namespace lilia::engine
