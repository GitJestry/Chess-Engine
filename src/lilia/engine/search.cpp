#include "lilia/engine/search.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <exception>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "lilia/engine/config.hpp"
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

// konservative Futility-Margen (idx = depth)
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
  void doNull() { applied = pos.doNullMove(); }
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
inline int mvv_lva_fast(const model::Position& pos, const model::Move& m) {
  if (!m.isCapture && m.promotion == core::PieceType::None) return 0;
  const auto& b = pos.getBoard();

  core::PieceType victimType = core::PieceType::Pawn;
  if (m.isEnPassant) {
    victimType = core::PieceType::Pawn;
  } else if (auto vp = b.getPiece(m.to)) {
    victimType = vp->type;
  }

  core::PieceType attackerType = core::PieceType::Pawn;
  if (auto ap = b.getPiece(m.from)) attackerType = ap->type;

  const int vVictim = base_value[static_cast<int>(victimType)];
  const int vAttacker = base_value[static_cast<int>(attackerType)];

  int score = (vVictim << 5) - vAttacker;  // *32 Spreizung

  if (m.promotion != core::PieceType::None) {
    static constexpr int promo_order[7] = {0, 40, 40, 60, 120, 0, 0};
    score += promo_order[static_cast<int>(m.promotion)];
  }
  if (m.isEnPassant) score += 5;
  return score;
}

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
  // grobe, aber monotone Steigerung per log2(depth^2 + 1)
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

static inline bool safeGenerateMoves(model::MoveGenerator& mg, model::Position& pos,
                                     std::vector<model::Move>& out) {
  if (out.capacity() < 128) out.reserve(128);
  out.clear();
  try {
    mg.generatePseudoLegalMoves(pos.getBoard(), pos.getState(), out);
    return true;
  } catch (const SearchStoppedException&) {
    throw;
  } catch (const std::exception& e) {
    std::cerr << "[Search] movegen exception: " << e.what() << '\n';
    out.clear();
    return false;
  } catch (...) {
    std::cerr << "[Search] movegen unknown exception\n";
    out.clear();
    return false;
  }
}

static inline bool safeGenerateCaptures(model::MoveGenerator& mg, model::Position& pos,
                                        std::vector<model::Move>& out) {
  if (out.capacity() < 128) out.reserve(128);
  out.clear();
  try {
    mg.generateCapturesOnly(pos.getBoard(), pos.getState(), out);
    return true;
  } catch (const SearchStoppedException&) {
    throw;
  } catch (const std::exception& e) {
    std::cerr << "[Search] capgen exception: " << e.what() << '\n';
    out.clear();
    return false;
  } catch (...) {
    std::cerr << "[Search] capgen unknown exception\n";
    out.clear();
    return false;
  }
}

static inline bool safeGenerateEvasions(model::MoveGenerator& mg, model::Position& pos,
                                        std::vector<model::Move>& out) {
  if (out.capacity() < 128) out.reserve(128);
  out.clear();
  try {
    mg.generateEvasions(pos.getBoard(), pos.getState(), out);
    return true;
  } catch (const SearchStoppedException&) {
    throw;
  } catch (...) {
    out.clear();
    return false;
  }
}

}  // namespace

// ---------- Search ----------

Search::Search(model::TT4& tt_, std::shared_ptr<const Evaluator> eval_, const EngineConfig& cfg_)
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

  for (auto& v : genBuf_) v.reserve(96);
  for (auto& v : legalBuf_) v.reserve(96);

  stopFlag.reset();
  stats = SearchStats{};
}

int Search::signed_eval(model::Position& pos) {
  int v = eval_->evaluate(pos);
  if (pos.getState().sideToMove == core::Color::Black) v = -v;
  return std::clamp(v, -MATE + 1, MATE - 1);
}

// ---------- Quiescence + QTT ----------

int Search::quiescence(model::Position& pos, int alpha, int beta, int ply) {
  stats.nodes++;
  check_stop(stopFlag);

  if (ply >= MAX_PLY - 2) return signed_eval(pos);

  const int kply = cap_ply(ply);
  const uint64_t parentKey = pos.hash();  // Parent-Key für TT
  const int alphaOrig = alpha, betaOrig = beta;

  // QSearch-TT Probe (depth==0)
  {
    model::TTEntry4 tte{};
    if (tt.probe_into(pos.hash(), tte)) {
      const int ttVal = decode_tt_score(tte.value, kply);
      if (tte.depth == 0) {
        if (tte.bound == model::Bound::Exact) return ttVal;
        if (tte.bound == model::Bound::Lower && ttVal >= beta) return ttVal;
        if (tte.bound == model::Bound::Upper && ttVal <= alpha) return ttVal;
      }
    }
  }

  if (pos.inCheck()) {
    // Evasions-only im Schach
    auto& pseudo = genBuf_[kply];
    if (!safeGenerateEvasions(mg, pos, pseudo)) return mated_in(ply);

    constexpr int MAXM = 256;
    int n = (int)std::min<size_t>(pseudo.size(), MAXM);
    int scores[MAXM];
    model::Move ordered[MAXM];

    const model::Move prev = (ply > 0 ? prevMove[cap_ply(ply - 1)] : model::Move{});
    const bool prevOk = (prev.from >= 0 && prev.to >= 0 && prev.from < 64 && prev.to < 64);
    const model::Move cm = prevOk ? counterMove[prev.from][prev.to] : model::Move{};

    for (int i = 0; i < n; ++i) {
      const auto& m = pseudo[i];
      int s = 0;
      if (prevOk && m == cm) s += 80'000;
      if (m.isCapture) s += 100'000 + mvv_lva_fast(pos, m);
      if (m.promotion != core::PieceType::None) s += 60'000;
      s += history[m.from][m.to];
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

    // Keine legalen Evasions -> matt
    if (!anyLegal) {
      const int ms = mated_in(ply);
      if (!(stopFlag && stopFlag->load()))
        tt.store(parentKey, encode_tt_score(ms, kply), 0, model::Bound::Exact, model::Move{});
      return ms;
    }

    // QTT Store (Exact/Upper/Lower)
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

  auto& qmoves = legalBuf_[kply];
  if (!safeGenerateCaptures(mg, pos, qmoves)) {
    if (!(stopFlag && stopFlag->load()))
      tt.store(parentKey, encode_tt_score(stand, kply), 0, model::Bound::Exact, model::Move{});
    return stand;
  }

  std::sort(qmoves.begin(), qmoves.end(), [&](const model::Move& a, const model::Move& b) {
    return mvv_lva_fast(pos, a) > mvv_lva_fast(pos, b);
  });

  constexpr int DELTA_MARGIN = 96;  // konservativ
  int best = stand;

  for (auto& m : qmoves) {
    check_stop(stopFlag);

    // Delta/SEE: SEE nur einmal, ordering ist SEE-frei
    if (m.isCapture && !pos.see(m) && m.promotion == core::PieceType::None) continue;

    const bool isQuietPromo = (m.promotion != core::PieceType::None) && !m.isCapture;

    int capVal = 0;
    if (m.isEnPassant)
      capVal = base_value[(int)core::PieceType::Pawn];
    else if (m.isCapture) {
      if (auto cap = pos.getBoard().getPiece(m.to)) capVal = base_value[(int)cap->type];
    }

    int promoGain = 0;
    if (m.promotion != core::PieceType::None)
      promoGain =
          std::max(0, base_value[(int)m.promotion] - base_value[(int)core::PieceType::Pawn]);

    if (!isQuietPromo) {
      if (stand + capVal + promoGain + DELTA_MARGIN <= alpha) continue;  // Delta Pruning
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
                    model::Move& refBest) {
  stats.nodes++;
  check_stop(stopFlag);

  if (ply >= MAX_PLY - 2) return signed_eval(pos);
  if (pos.checkInsufficientMaterial() || pos.checkMoveRule() || pos.checkRepetition()) return 0;
  if (depth <= 0) return quiescence(pos, alpha, beta, ply);

  // Mate Distance Pruning
  alpha = std::max(alpha, mated_in(ply));
  beta = std::min(beta, mate_in(ply));
  if (alpha >= beta) return alpha;

  const int origAlpha = alpha;
  const int origBeta = beta;
  const bool isPV = (beta - alpha > 1);

  const bool inCheck = pos.inCheck();
  const int staticEval = inCheck ? 0 : signed_eval(pos);

  // --- SNMP (Static Null-Move Pruning) für depth <= 3 (nicht PV, nicht in Check), nur im
  // Mittelspiel ---
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
      static constexpr int margins[4] = {0, 140, 200, 260};  // idx==depth
      if (staticEval - margins[depth] >= beta) return staticEval;
    }
  }

  // --- SAFER RAZORING (nur Depth==1, nie im PV) ---
  if (!inCheck && !isPV && depth == 1) {
    const int razorMargin = 220;
    if (staticEval + razorMargin <= alpha) {
      int q = quiescence(pos, alpha - 1, alpha, ply);
      if (q <= alpha) return q;
    }
  }

  // --- REVERSE FUTILITY (nur Depth==1, nie PV) ---
  if (!inCheck && !isPV && depth == 1) {
    const int margin = 180;
    if (staticEval - margin >= beta) return staticEval;  // fail-soft approx
  }

  int best = -INF;
  model::Move bestLocal{};

  // --- TT-Probe ---
  model::Move ttMove{};
  bool haveTT = false;
  int ttVal = 0;
  model::TTEntry4 tte{};
  if (tt.probe_into(pos.hash(), tte)) {
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

  // IID (sanft, nicht im Check)
  if (!haveTT && depth >= 5 && !inCheck) {
    model::Move tmp{};
    (void)negamax(pos, depth - 2, alpha, beta, ply, tmp);
    model::TTEntry4 tte2{};
    if (tt.probe_into(pos.hash(), tte2)) {
      MoveUndoGuard g(pos);
      if (g.doMove(tte2.best)) {
        haveTT = true;
        ttMove = tte2.best;
      }
    }
  }

  // --- NULL MOVE: adaptiver Trigger + optionale Verification ---
  auto nonPawnCount = [&](const model::Board& b) {
    using PT = core::PieceType;
    auto countSide = [&](core::Color c) {
      return model::bb::popcount(b.getPieces(c, PT::Knight) | b.getPieces(c, PT::Bishop) |
                                 b.getPieces(c, PT::Rook) | b.getPieces(c, PT::Queen));
    };
    return countSide(core::Color::White) + countSide(core::Color::Black);
  };
  const bool sparse = (nonPawnCount(pos.getBoard()) <= 3);
  const bool prevWasCapture = (ply > 0 && prevMove[cap_ply(ply - 1)].isCapture);

  if (cfg.useNullMove && depth >= 3 && !inCheck && !isPV && !sparse && !prevWasCapture) {
    const int margin = 50 + 20 * depth;  // adaptiv
    if (staticEval >= beta + margin) {
      NullUndoGuard ng(pos);
      ng.doNull();
      if (ng.applied) {
        int R = (depth >= 7 ? 3 : 2);
        int nullScore = -negamax(pos, depth - 1 - R, -beta, -beta + 1, ply + 1, refBest);
        if (nullScore >= beta) {
          if (depth >= 6) {
            int verify = -negamax(pos, depth - 1 - R, -beta, -beta + 1, ply + 1, refBest);
            if (verify >= beta) return beta;
          } else {
            return beta;
          }
        }
      }
    }
  }

  const int kply = cap_ply(ply);
  auto& gen = genBuf_[kply];

  gen.clear();
  if (inCheck) {
    if (!safeGenerateEvasions(mg, pos, gen)) {
      return mated_in(ply);
    }
  } else if (!safeGenerateMoves(mg, pos, gen)) {
    if (inCheck) return mated_in(ply);
    return 0;
  }
  if (gen.empty()) {
    if (inCheck) return mated_in(ply);
    return 0;
  }

  // prev für CounterMove
  const model::Move prev = (ply > 0 ? prevMove[cap_ply(ply - 1)] : model::Move{});
  const bool prevOk = (prev.from >= 0 && prev.to >= 0 && prev.from < 64 && prev.to < 64);
  const model::Move cm = prevOk ? counterMove[prev.from][prev.to] : model::Move{};

  // Move Ordering direkt auf Pseudo-Moves (SEE-frei)
  constexpr int MAX_MOVES = 256;
  int n = (int)std::min<size_t>(gen.size(), MAX_MOVES);

  int scores[MAX_MOVES];
  model::Move ordered[MAX_MOVES];

  constexpr int TT_BONUS = 2'400'000;
  constexpr int CAP_BASE_GOOD = 180'000;
  constexpr int PROMO_BASE = 160'000;
  constexpr int KILLER_BASE = 120'000;
  constexpr int CM_BASE = 140'000;

  const auto& board = pos.getBoard();

  for (int i = 0; i < n; ++i) {
    const auto& m = gen[i];
    int s = 0;

    if (haveTT && m == ttMove) {
      s = TT_BONUS;
    } else if (m.isCapture || m.promotion != core::PieceType::None) {
      auto moverOpt = board.getPiece(m.from);
      const core::PieceType moverPt = moverOpt ? moverOpt->type : core::PieceType::Pawn;
      core::PieceType capPt = core::PieceType::Pawn;
      if (m.isEnPassant)
        capPt = core::PieceType::Pawn;
      else if (auto cap = board.getPiece(m.to))
        capPt = cap->type;

      const int mvv = mvv_lva_fast(pos, m);
      const int ch = captureHist[pidx(moverPt)][m.to][pidx(capPt)];
      if (m.promotion != core::PieceType::None && !m.isCapture)
        s = PROMO_BASE + mvv;
      else
        s = CAP_BASE_GOOD + mvv + (ch >> 2);
    } else {
      auto moverOpt = board.getPiece(m.from);
      const core::PieceType moverPt = moverOpt ? moverOpt->type : core::PieceType::Pawn;
      s = history[m.from][m.to] + (quietHist[pidx(moverPt)][m.to] >> 1);
      if (m == killers[kply][0] || m == killers[kply][1]) s += KILLER_BASE;
      if (prevOk && m == cm) s += CM_BASE + (counterHist[prev.from][prev.to] >> 1);
    }

    scores[i] = s;
    ordered[i] = m;
  }
  sort_by_score_desc(scores, ordered, n);

  // PVS + LMR + LMP + Futility + SEE-Pruning + ProbCut
  const bool allowFutility = !inCheck && !isPV;
  int moveCount = 0;

  for (int idx = 0; idx < n; ++idx) {
    check_stop(stopFlag);

    const model::Move m = ordered[idx];
    const bool isQuiet = !m.isCapture && (m.promotion == core::PieceType::None);

    // Parent-Infos (pre)
    auto moverOpt = board.getPiece(m.from);
    const core::PieceType moverPt = moverOpt ? moverOpt->type : core::PieceType::Pawn;
    core::PieceType capPt = core::PieceType::Pawn;
    if (m.isEnPassant)
      capPt = core::PieceType::Pawn;
    else if (m.isCapture) {
      if (auto cap = board.getPiece(m.to)) capPt = cap->type;
    }

    // --- LMP konservativer ---
    if (!inCheck && !isPV && isQuiet && depth <= 3) {
      int limit = depth * depth;  // 1,4,9
      int h = history[m.from][m.to] + (quietHist[pidx(moverPt)][m.to] >> 1);
      if (h < -8000) limit = std::max(1, limit - 1);
      if (staticEval + FUT_MARGIN[depth] <= alpha + 32 && moveCount >= limit) {
        ++moveCount;
        continue;
      }
    }

    // --- Extended Futility (depth <= 3, nur quiets) ---
    if (allowFutility && isQuiet && depth <= 3) {
      int fut = FUT_MARGIN[depth] + (history[m.from][m.to] < -8000 ? 32 : 0);
      if (staticEval + fut <= alpha) {
        ++moveCount;
        continue;
      }
    }

    // --- HISTORY PRUNING (leicht schärfer) ---
    if (!inCheck && !isPV && isQuiet && depth <= 2) {
      int histScore = history[m.from][m.to] + (quietHist[pidx(moverPt)][m.to] >> 1);
      if (histScore < -11000 && m != killers[kply][0] && m != killers[kply][1] &&
          (!prevOk || m != cm)) {
        ++moveCount;
        continue;
      }
    }

    // --- FUTILITY (nur Depth==1, nie PV) ---
    if (!inCheck && !isPV && isQuiet && depth == 1) {
      if (staticEval + 110 <= alpha) {
        ++moveCount;
        continue;
      }
    }

    // SEE einmalig (erst nach LMP/Futility)
    bool seeGood = true;
    if (!inCheck && ply > 0 && m.isCapture && depth <= 5) {
      if (!pos.see(m)) {
        ++moveCount;
        continue;
      }
    } else if (m.isCapture) {
      seeGood = pos.see(m);
    }

    const int mvvBefore =
        (m.isCapture || m.promotion != core::PieceType::None) ? mvv_lva_fast(pos, m) : 0;

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

    // --- ProbCut: non-PV, gutes Capture, statEval nahe beta ---
    if (!isPV && !inCheck && depth >= 5 && m.isCapture && seeGood && mvvBefore >= 700) {
      int capVal = 0;
      if (m.isEnPassant)
        capVal = base_value[(int)core::PieceType::Pawn];
      else if (auto cap = board.getPiece(m.to))
        capVal = base_value[(int)cap->type];
      const int PROBCUT_MARGIN = 180;
      if (staticEval + capVal + PROBCUT_MARGIN >= beta) {
        const int red = 3;
        const int probe = -negamax(pos, depth - red, -beta, -(beta - 1), ply + 1, childBest);
        if (probe >= beta) return beta;
      }
    }

    // Check-Extension (leicht)
    const bool givesCheck = pos.inCheck();
    if (givesCheck && (isQuiet || seeGood)) newDepth += 1;

    // Bad-Capture-Reduction (BCR)
    int reduction = 0;
    if (!seeGood && m.isCapture && newDepth >= 2) reduction = std::min(1, newDepth - 1);

    if (moveCount == 0) {
      // PVS Vollfenster
      value = -negamax(pos, newDepth, -beta, -alpha, ply + 1, childBest);
    } else {
      // LMR (sanft, caps & checks nicht reduzieren); Cap bis 3
      if (cfg.useLMR && isQuiet && !inCheck && !givesCheck && newDepth >= 2 && moveCount >= 3) {
        const int ld = ilog2_u32((unsigned)depth);
        const int lm = ilog2_u32((unsigned)(moveCount + 1));
        int rBase = (ld * (lm + 1)) / 3;  // sanftere Grundreduktion

        const int h = history[m.from][m.to] + (quietHist[pidx(moverPt)][m.to] >> 1);
        if (h > 8000) rBase -= 1;   // gute Historie → weniger Reduktion
        if (h < -8000) rBase += 1;  // schlechte Historie → mehr Reduktion

        if (m == killers[kply][0] || m == killers[kply][1]) rBase -= 1;
        if (haveTT && m == ttMove) rBase -= 1;
        if (prevOk && m == cm) rBase -= 1;

        if (ply <= 2) rBase -= 1;           // nahe Root
        if (beta - alpha <= 8) rBase -= 1;  // enge Fenster

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

    // History/Heuristik-Updates
    if (isQuiet) {
      if (value <= origAlpha) {
        hist_update(history[m.from][m.to], -hist_bonus(depth) / 2);
        auto moverOpt2 = board.getPiece(m.from);  // moverPt bereits vor Move erfasst
        hist_update(quietHist[pidx(moverPt)][m.to], -hist_bonus(depth) / 2);
      }
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
        hist_update(history[m.from][m.to], +hist_bonus(depth));
        hist_update(quietHist[pidx(moverPt)][m.to], +hist_bonus(depth));

        if (prevOk) {
          counterMove[prev.from][prev.to] = m;
          hist_update(counterHist[prev.from][prev.to], +hist_bonus(depth));
        }
      } else {
        hist_update(captureHist[pidx(moverPt)][m.to][pidx(capPt)], +hist_bonus(depth));
      }
      break;
    }
    ++moveCount;
  }

  if (best == -INF) {  // kein legaler Zug
    if (inCheck) return mated_in(ply);
    return 0;
  }

  // TT-Store
  if (!(stopFlag && stopFlag->load())) {
    model::Bound bound;
    if (best <= origAlpha)
      bound = model::Bound::Upper;
    else if (best >= origBeta)
      bound = model::Bound::Lower;
    else
      bound = model::Bound::Exact;

    try {
      tt.store(pos.hash(), encode_tt_score(best, cap_ply(ply)), static_cast<int16_t>(depth), bound,
               bestLocal);
    } catch (...) {
    }
  }

  refBest = bestLocal;
  return best;
}

// ---------- PV aus TT ----------

std::vector<model::Move> Search::build_pv_from_tt(model::Position pos, int max_len) {
  std::vector<model::Move> pv;
  for (int i = 0; i < max_len; ++i) {
    model::TTEntry4 tte{};
    if (!tt.probe_into(pos.hash(), tte)) break;
    model::Move m = tte.best;

    if (m.from == m.to) break;
    if (m.from < 0 || m.from >= 64 || m.to < 0 || m.to >= 64) break;

    auto pieceOpt = pos.getBoard().getPiece(m.from);
    if (!pieceOpt) break;
    if (pieceOpt->color != pos.getState().sideToMove) break;

    const bool isPawn = (pieceOpt->type == core::PieceType::Pawn);
    const int toRank = ((int)m.to) >> 3;
    const bool onPromoRank =
        (pieceOpt->color == core::Color::White) ? (toRank == 7) : (toRank == 0);
    if (isPawn && onPromoRank && m.promotion == core::PieceType::None) break;

    if (!pos.doMove(m)) break;
    pv.push_back(m);
  }
  return pv;
}

// ---------- Root Search (parallel, Work-Queue, fixes) ----------

int Search::search_root_parallel(model::Position& pos, int maxDepth,
                                 std::shared_ptr<std::atomic<bool>> stop, int maxThreads) {
  this->stopFlag = stop;
  stats = SearchStats{};

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
  if (!safeGenerateMoves(mg, pos, rootMoves) || rootMoves.empty()) {
    update_time_stats();
    this->stopFlag.reset();
    return 0;
  }

  // Thread-Pool setup
  auto& pool = ThreadPool::instance(maxThreads);
  const int hw = (int)std::thread::hardware_concurrency();
  const int threads = std::max(1, maxThreads > 0 ? maxThreads : (hw > 0 ? hw : 1));
  pool.maybe_resize(threads);

  // Aspiration seed
  int lastScoreGuess = 0;
  if (cfg.useAspiration) {
    model::TTEntry4 tte{};
    if (tt.probe_into(pos.hash(), tte)) lastScoreGuess = decode_tt_score(tte.value, 0);
  }

  auto score_root_move = [&](const model::Move& m, const model::Move& ttMove, bool haveTT) {
    int s = 0;
    if (haveTT && m == ttMove) s += 2'500'000;
    if (m.isCapture) s += 1'100'000 + mvv_lva_fast(pos, m);
    if (m.promotion != core::PieceType::None) s += 1'050'000;
    s += history[m.from][m.to];
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
    if (model::TTEntry4 tte{}; tt.probe_into(pos.hash(), tte)) {
      haveRootTT = true;
      rootTT = tte.best;
    }

    std::sort(rootMoves.begin(), rootMoves.end(), [&](const model::Move& a, const model::Move& b) {
      int sa = score_root_move(a, rootTT, haveRootTT);
      int sb = score_root_move(b, rootTT, haveRootTT);
      if (sa != sb) return sa > sb;
      if (a.from != b.from) return a.from < b.from;
      return a.to < b.to;
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
        Search worker(tt, eval_, cfg);  // eigener Worker
        worker.stopFlag = stop;
        worker.prevMove[0] = m0;  // CounterHistory
        model::Move ref{};

        int s = 0;
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
        s = std::clamp(s, -MATE + 1, MATE - 1);

        results[0].nullScore.store(s, std::memory_order_relaxed);
        results[0].fullScore.store(s, std::memory_order_relaxed);
        sharedAlpha.store(s, std::memory_order_relaxed);

        stats.nodes += worker.getStats().nodes;
      }
    }

    // 2) Restliche Root-Moves per Work-Queue
    const int total = (int)rootMoves.size();
    std::atomic<int> idx{1};  // Move 0 ist schon erledigt

    std::atomic<uint64_t> nodesAcc{0};

    auto workerFn = [&](int /*threadId*/) {
      // Ein Worker-Search, wiederverwendet für viele Moves → spart Allokationen
      Search worker(tt, eval_, cfg);
      worker.stopFlag = stop;

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

        // Nullfenster
        uint64_t before = worker.getStats().nodes;
        int sN = -worker.negamax(child, depth - 1, -(localAlpha + 1), -localAlpha, 1, ref);
        sN = std::clamp(sN, -MATE + 1, MATE - 1);
        nodesAcc.fetch_add(worker.getStats().nodes - before, std::memory_order_relaxed);

        results[i].nullScore.store(sN, std::memory_order_relaxed);

        // Ggf. Full-Window bestätigen
        if (sN >= localAlpha && !(stop && stop->load())) {
          before = worker.getStats().nodes;
          int sF = -worker.negamax(child, depth - 1, -INF, INF, 1, ref);
          sF = std::clamp(sF, -MATE + 1, MATE - 1);
          nodesAcc.fetch_add(worker.getStats().nodes - before, std::memory_order_relaxed);

          results[i].fullScore.store(sF, std::memory_order_relaxed);

          // sharedAlpha erhöhen, falls besser
          int cur = localAlpha;
          while (sF > cur &&
                 !sharedAlpha.compare_exchange_weak(cur, sF, std::memory_order_relaxed)) {
          }
        }
      }
    };

    // Starte N-1 Worker
    const int workers = std::min(total - 1, threads - 1);
    std::vector<std::future<void>> futs;
    futs.reserve(workers);
    for (int t = 0; t < workers; ++t) {
      futs.emplace_back(pool.submit([&, tid = t] { workerFn(tid); }));
    }

    // Warten
    for (auto& f : futs) {
      try {
        f.get();
      } catch (...) {
      }
    }
    stats.nodes += nodesAcc.load(std::memory_order_relaxed);

    // 3) Confirm-Pass NUR für Moves, die Nullfenster gewonnen haben,
    //    aber noch keinen Full-Score besitzen (sehr wenige).
    const int bestAlphaNow = sharedAlpha.load(std::memory_order_relaxed);
    for (int i = 1; i < total; ++i) {
      if (stop && stop->load()) break;
      const int sN = results[i].nullScore.load(std::memory_order_relaxed);
      const int sF = results[i].fullScore.load(std::memory_order_relaxed);
      if (sF != std::numeric_limits<int>::min()) continue;  // schon bestätigt
      if (sN < bestAlphaNow) continue;                      // kein Winner

      model::Position child = pos;
      if (!child.doMove(rootMoves[i])) continue;

      Search worker(tt, eval_, cfg);
      worker.stopFlag = stop;
      worker.prevMove[0] = rootMoves[i];
      model::Move ref{};
      uint64_t before = worker.getStats().nodes;
      int sF2 = -worker.negamax(child, depth - 1, -INF, INF, 1, ref);
      sF2 = std::clamp(sF2, -MATE + 1, MATE - 1);
      results[i].fullScore.store(sF2, std::memory_order_relaxed);
      stats.nodes += worker.getStats().nodes - before;

      int cur = sharedAlpha.load(std::memory_order_relaxed);
      while (sF2 > cur && !sharedAlpha.compare_exchange_weak(cur, sF2, std::memory_order_relaxed)) {
      }
    }

    // 4) Ranking
    std::vector<std::pair<int, model::Move>> depthCand;
    depthCand.reserve(total);
    for (int i = 0; i < total; ++i) {
      int sF = results[i].fullScore.load(std::memory_order_relaxed);
      int sN = results[i].nullScore.load(std::memory_order_relaxed);
      int s = (sF != std::numeric_limits<int>::min()) ? sF : sN;
      depthCand.emplace_back(s, rootMoves[i]);
    }

    int NDISPLAY = (depth >= 6 ? 2 : 1);
    NDISPLAY = std::min<int>(NDISPLAY, (int)depthCand.size());
    std::partial_sort(depthCand.begin(), depthCand.begin() + std::max(1, NDISPLAY), depthCand.end(),
                      [](const auto& a, const auto& b) { return a.first > b.first; });

    std::sort(depthCand.begin(), depthCand.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    // Update stats/time
    update_time_stats();

    // PV/Top
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

    stats.topMoves.clear();
    const int SHOW = std::min<int>(5, (int)depthCand.size());
    for (int i = 0; i < SHOW; ++i) {
      stats.topMoves.push_back(
          {depthCand[i].second, std::clamp(depthCand[i].first, -MATE + 1, MATE - 1)});
    }

    if (is_mate_score(stats.bestScore)) break;

    // Aspiration seed für nächste Iteration
    lastScoreGuess = bestScore;
  }

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
  stats = SearchStats{};
}

}  // namespace lilia::engine
