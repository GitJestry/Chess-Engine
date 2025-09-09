#include "lilia/engine/search.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
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
#include "lilia/model/core/bitboard.hpp"
#include "lilia/model/core/magic.hpp"

namespace lilia::engine {

using steady_clock = std::chrono::steady_clock;

// ---------- kleine Helfer / Konstanten ----------
inline int16_t clamp16(int x) {
  if (x > 32767) return 32767;
  if (x < -32768) return -32768;
  return (int16_t)x;
}

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
static constexpr int ROOT_VERIFY_MARGIN_BASE = 60;
static constexpr int FUT_MARGIN[4] = {0, 110, 210, 300};  // leicht angehoben
static constexpr int SNMP_MARGINS[4] = {0, 140, 200, 260};
static constexpr int RAZOR_MARGIN_BASE = 240;  // vorher 220
static constexpr int RFP_MARGIN_BASE = 190;    // vorher 180
// LMP-Limits pro Tiefe (nur Quiet-Züge)
static constexpr int LMP_LIMIT[4] = {0, 5, 9, 14};  // D=1..3

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

static inline void decay_tables(Search& S, int shift /* e.g. 6 => ~1.6% */) {
  for (int f = 0; f < SQ_NB; ++f)
    for (int t = 0; t < SQ_NB; ++t)
      S.history[f][t] = clamp16((int)S.history[f][t] - ((int)S.history[f][t] >> shift));

  for (int p = 0; p < PIECE_NB; ++p)
    for (int t = 0; t < SQ_NB; ++t)
      S.quietHist[p][t] = clamp16((int)S.quietHist[p][t] - ((int)S.quietHist[p][t] >> shift));

  for (int mp = 0; mp < PIECE_NB; ++mp)
    for (int t = 0; t < SQ_NB; ++t)
      for (int cp = 0; cp < PIECE_NB; ++cp)
        S.captureHist[mp][t][cp] =
            clamp16((int)S.captureHist[mp][t][cp] - ((int)S.captureHist[mp][t][cp] >> shift));

  for (int f = 0; f < SQ_NB; ++f)
    for (int t = 0; t < SQ_NB; ++t)
      S.counterHist[f][t] = clamp16((int)S.counterHist[f][t] - ((int)S.counterHist[f][t] >> shift));

  for (int L = 0; L < CH_LAYERS; ++L)
    for (int pp = 0; pp < PIECE_NB; ++pp)
      for (int pt = 0; pt < SQ_NB; ++pt)
        for (int mp = 0; mp < PIECE_NB; ++mp)
          for (int to = 0; to < SQ_NB; ++to) {
            int16_t& h = S.contHist[L][pp][pt][mp][to];
            h = clamp16((int)h - ((int)h >> shift));
          }
}

// 0 = no signal; 1 = attacks high-value piece; 2 = gives check
static inline int quiet_piece_threat_signal(const model::Board& b, const model::Move& m,
                                            core::Color us) {
  using PT = core::PieceType;
  if (m.isCapture() || m.promotion() != PT::None) return 0;

  auto mover = b.getPiece(m.from());
  if (!mover || mover->type == PT::Pawn) return 0;

  const auto fromBB = model::bb::sq_bb(m.from());
  const auto toBB = model::bb::sq_bb(m.to());
  auto occ = b.getAllPieces();
  occ = (occ & ~fromBB) | toBB;

  model::bb::Bitboard atk = 0;
  switch (mover->type) {
    case PT::Knight:
      atk = model::bb::knight_attacks_from(m.to());
      break;
    case PT::Bishop:
      atk = model::magic::sliding_attacks(model::magic::Slider::Bishop, m.to(), occ);
      break;
    case PT::Rook:
      atk = model::magic::sliding_attacks(model::magic::Slider::Rook, m.to(), occ);
      break;
    case PT::Queen:
      atk = model::magic::sliding_attacks(model::magic::Slider::Bishop, m.to(), occ) |
            model::magic::sliding_attacks(model::magic::Slider::Rook, m.to(), occ);
      break;
    case PT::King:
      atk = model::bb::king_attacks_from(m.to());
      break;
    default:
      break;
  }

  if (atk & b.getPieces(~us, PT::King)) return 2;

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
  std::memset(contHist, 0, sizeof(contHist));
  for (auto& row : counterMove)
    for (auto& m : row) m = model::Move{};
  for (auto& pm : prevMove) pm = model::Move{};

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
  // Batch: nur alle 1024 Knoten atomar addieren + Stop prüfen
  static thread_local uint32_t local = 0;
  constexpr uint32_t TICK_STEP = 1024;

  // schneller Hot-Path: nur Zähler hochzählen
  ++local;

  // alle 64 Knoten einmal billig auf stopFlag schauen (nur relaxed load)
  if ((local & 63u) == 0u) {
    if (stopFlag && stopFlag->load(std::memory_order_relaxed)) {
      throw SearchStoppedException();
    }
  }

  // seltener Slow-Path: Batch flushen + Limit prüfen
  if ((local & (TICK_STEP - 1u)) == 0u) {
    if (counter) {
      std::uint64_t cur = counter->fetch_add(TICK_STEP, std::memory_order_relaxed) + TICK_STEP;
      if (limit && cur >= limit) {
        if (stopFlag) stopFlag->store(true, std::memory_order_relaxed);
        throw SearchStoppedException();
      }
    }
    if (stopFlag && stopFlag->load(std::memory_order_relaxed)) {
      throw SearchStoppedException();
    }
  }
}

// ---------- Quiescence + QTT ----------

int Search::quiescence(model::Position& pos, int alpha, int beta, int ply) {
  bump_node_or_stop(sharedNodes, nodeLimit, stopFlag);

  if (ply >= MAX_PLY - 2) return signed_eval(pos);

  const int kply = cap_ply(ply);
  const uint64_t parentKey = pos.hash();
  const int alphaOrig = alpha, betaOrig = beta;

  model::Move bestMoveQ{};  // <- track best move we actually play in qsearch

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
  const bool inCheck = pos.inCheck();
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
      if ((i & 63) == 0) check_stop(stopFlag);
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
          tt.store(parentKey, encode_tt_score(beta, kply), 0, model::Bound::Lower,
                   m);  // store cutoff move
        return beta;
      }
      if (score > best) {
        best = score;
        bestMoveQ = m;
      }
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
      tt.store(parentKey, encode_tt_score(best, kply), 0, b, bestMoveQ);
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
  // append non-capture promotions
  if (qn < engine::MAX_MOVES) {
    engine::MoveBuffer buf(capArr_[kply] + qn, engine::MAX_MOVES - qn);
    qn += mg.generateNonCapturePromotions(pos.getBoard(), pos.getState(), buf);
  }

  // Captures sortieren via precomputed MVV-LVA Scores
  constexpr int MAXM = engine::MAX_MOVES;
  int qs[MAXM];
  model::Move qord[MAXM];
  for (int i = 0; i < qn; ++i) {
    const auto& m = capArr_[kply][i];
    qs[i] = mvv_lva_fast(pos, m);
    qord[i] = m;
  }
  sort_by_score_desc(qs, qord, qn);

  // etwas schärfer
  constexpr int DELTA_MARGIN = 112;
  int best = stand;

  for (int i = 0; i < qn; ++i) {
    const model::Move m = qord[i];
    if ((i & 63) == 0) check_stop(stopFlag);

    const bool isCap = m.isCapture();
    const bool isPromo = (m.promotion() != core::PieceType::None);
    const int mvv = (isCap || isPromo) ? mvv_lva_fast(pos, m) : 0;

    // ---- SEE nur EINMAL bestimmen (falls nötig) ----
    bool seeOk = true;
    if (isCap && !isPromo) {
      // Nur wenn materiell "verdächtig": Opfer < Angreifer
      const auto moverOptQ = pos.getBoard().getPiece(m.from());
      const core::PieceType attackerPtQ = moverOptQ ? moverOptQ->type : core::PieceType::Pawn;
      const int attackerValQ = base_value[(int)attackerPtQ];

      int victimValQ = 0;
      if (m.isEnPassant()) {
        victimValQ = base_value[(int)core::PieceType::Pawn];
      } else if (auto capQ = pos.getBoard().getPiece(m.to())) {
        victimValQ = base_value[(int)capQ->type];
      }

      if (victimValQ < attackerValQ) {
        seeOk = pos.see(m);  // genau EIN Aufruf
        // sehr schlechte, SEE-negative Klein-Captures sofort verwerfen
        if (!seeOk && mvv < 400) continue;
      }
    }
    // -----------------------------------------------

    // ---- Safer delta pruning ----
    bool maybeCheck = false;
    if (isPromo) {
      maybeCheck = true;
    } else {
      auto us = pos.getState().sideToMove;
      const auto toBB = model::bb::sq_bb(m.to());
      const auto kBB = pos.getBoard().getPieces(~us, core::PieceType::King);
      if (isCap) {
        if (us == core::Color::White) {
          if ((model::bb::ne(toBB) | model::bb::nw(toBB)) & kBB) maybeCheck = true;
        } else {
          if ((model::bb::se(toBB) | model::bb::sw(toBB)) & kBB) maybeCheck = true;
        }
      }
    }

    if (!maybeCheck) {
      if (isCap || isPromo) {
        int capVal = 0;
        if (m.isEnPassant())
          capVal = base_value[(int)core::PieceType::Pawn];
        else if (isCap) {
          if (auto cap = pos.getBoard().getPiece(m.to())) capVal = base_value[(int)cap->type];
        }
        int promoGain = 0;
        if (isPromo)
          promoGain =
              std::max(0, base_value[(int)m.promotion()] - base_value[(int)core::PieceType::Pawn]);

        const bool quietPromo = isPromo && !isCap;
        if (quietPromo) {
          if (stand + promoGain + DELTA_MARGIN <= alpha) continue;
        } else {
          if (stand + capVal + promoGain + DELTA_MARGIN <= alpha) continue;
        }
      }
    }
    // --------------------------------------

    // (Kein zweites SEE mehr nötig – 'seeOk' ist gesetzt)

    MoveUndoGuard g(pos);
    if (!g.doMove(m)) continue;

    prevMove[cap_ply(ply)] = m;
    tt.prefetch(pos.hash());
    int score = -quiescence(pos, -beta, -alpha, ply + 1);
    score = std::clamp(score, -MATE + 1, MATE - 1);

    if (score >= beta) {
      if (!(stopFlag && stopFlag->load()))
        tt.store(parentKey, encode_tt_score(beta, kply), 0, model::Bound::Lower,
                 m);  // store cutoff move
      return beta;
    }
    if (score > alpha) alpha = score;
    if (score > best) {
      best = score;
      bestMoveQ = m;
    }
  }

  if (!(stopFlag && stopFlag->load())) {
    model::Bound b = model::Bound::Exact;
    if (best <= alphaOrig)
      b = model::Bound::Upper;
    else if (best >= betaOrig)
      b = model::Bound::Lower;
    tt.store(parentKey, encode_tt_score(best, kply), 0, b, bestMoveQ);
  }
  return best;
}

// ---------- Negamax ----------

int Search::negamax(model::Position& pos, int depth, int alpha, int beta, int ply,
                    model::Move& refBest, int parentStaticEval, const model::Move* excludedMove) {
  bump_node_or_stop(sharedNodes, nodeLimit, stopFlag);

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

  const bool inCheck = pos.inCheck();
  const int staticEval = inCheck ? 0 : signed_eval(pos);

  // ---- "improving" ----
  const bool improving =
      !inCheck && (parentStaticEval == INF || staticEval >= parentStaticEval - 16);

  // einmalig: Nicht-Bauern-Material zählen (für SNMP & Nullmove)
  const auto& b = pos.getBoard();
  auto countSide = [&](core::Color c) {
    using PT = core::PieceType;
    return model::bb::popcount(b.getPieces(c, PT::Knight) | b.getPieces(c, PT::Bishop) |
                               b.getPieces(c, PT::Rook) | b.getPieces(c, PT::Queen));
  };
  const int nonP = countSide(core::Color::White) + countSide(core::Color::Black);

  // SNMP
  if (!inCheck && !isPV && depth <= 3) {
    if (nonP >= 6) {
      int mar = SNMP_MARGINS[depth] + (improving ? 32 : 0);
      if (staticEval - mar >= beta) return staticEval;
    }
  }

  // Razoring D1
  if (!inCheck && !isPV && depth == 1) {
    int razorMargin = RAZOR_MARGIN_BASE + (improving ? 40 : 0);
    if (staticEval + razorMargin <= alpha) {
      int q = quiescence(pos, alpha - 1, alpha, ply);
      if (q <= alpha) return q;
    }
  }

  // Reverse futility D1
  if (!inCheck && !isPV && depth == 1) {
    int margin = RFP_MARGIN_BASE + (improving ? 40 : 0);
    if (staticEval - margin >= beta) return staticEval;
  }

  int best = -INF;
  model::Move bestLocal{};

  // ----- TT probe -----
  model::Move ttMove{};
  bool haveTT = false;
  int ttVal = 0;
  model::Bound ttBound = model::Bound::Upper;  // track for SE
  int ttStoredDepth = -1;

  if (model::TTEntry5 tte{}; tt.probe_into(pos.hash(), tte)) {
    haveTT = true;
    ttMove = tte.best;
    ttVal = decode_tt_score(tte.value, cap_ply(ply));
    ttBound = tte.bound;
    ttStoredDepth = (int)tte.depth;

    if (tte.depth >= depth) {
      if (tte.bound == model::Bound::Exact) return std::clamp(ttVal, -MATE + 1, MATE - 1);
      if (tte.bound == model::Bound::Lower) alpha = std::max(alpha, ttVal);
      if (tte.bound == model::Bound::Upper) beta = std::min(beta, ttVal);
      if (alpha >= beta) return std::clamp(ttVal, -MATE + 1, MATE - 1);
    }
  }

  // ----- IID -----
  if (cfg.useIID && !inCheck && depth >= 5) {
    const bool weakTT = !haveTT || (ttStoredDepth < depth - 2) || (ttMove.from() == ttMove.to());
    if (weakTT) {
      int iidDepth = isPV ? depth - 2 : depth - 3;
      if (iidDepth < 1) iidDepth = 1;

      int a = alpha, bnd = isPV ? beta : (alpha + 1);

      model::Move tmp{};
      (void)negamax(pos, iidDepth, a, bnd, ply, tmp, staticEval);

      if (model::TTEntry5 t2{}; tt.probe_into(pos.hash(), t2)) {
        haveTT = true;
        ttMove = t2.best;
        ttVal = decode_tt_score(t2.value, cap_ply(ply));
        ttBound = t2.bound;
        ttStoredDepth = (int)t2.depth;

        if (t2.depth >= depth - 1) {
          if (t2.bound == model::Bound::Exact) return std::clamp(ttVal, -MATE + 1, MATE - 1);

          if (t2.bound == model::Bound::Lower)
            alpha = std::max(alpha, ttVal);
          else if (t2.bound == model::Bound::Upper)
            beta = std::min(beta, ttVal);

          if (alpha >= beta) return std::clamp(ttVal, -MATE + 1, MATE - 1);
        }
      }
    }
  }

  // Null move pruning (adaptive)
  const bool sparse = (nonP <= 3);
  const bool prevWasCapture = (ply > 0 && prevMove[cap_ply(ply - 1)].isCapture());

  if (cfg.useNullMove && depth >= 3 && !inCheck && !isPV && !sparse && !prevWasCapture) {
    const int evalGap = staticEval - beta;
    int rBase = 2 + (depth >= 8 ? 1 : 0);  // 2..3
    if (evalGap > 200) rBase++;
    if (evalGap > 500) rBase++;
    if (!improving) rBase++;
    if (nonP >= 8) rBase++;

    int R = std::min(rBase, depth - 2);

    int margin = 50 + 20 * depth + (improving ? 40 : 0);
    if (staticEval >= beta + margin) {
      NullUndoGuard ng(pos);
      if (ng.doNull()) {
        model::Move tmpNM{};
        int nullScore = -negamax(pos, depth - 1 - R, -beta, -beta + 1, ply + 1, tmpNM, staticEval);
        ng.rollback();
        if (nullScore >= beta) {
          const bool needVerify = (depth >= 8 && R >= 3 && evalGap < 800);
          if (needVerify) {
            model::Move tmpVerify{};
            int verify = -negamax(pos, depth - 1, -beta, -beta + 1, ply + 1, tmpVerify, staticEval);
            if (verify >= beta) return beta;
          } else {
            return beta;
          }
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

      // NEW: tactical quiet piece/pawn bonuses (use signals)
      const auto us = pos.getState().sideToMove;
      const int pawn_sig = quiet_pawn_push_signal(board, m, us);
      const int piece_sig = quiet_piece_threat_signal(board, m, us);
      const int sig = pawn_sig > piece_sig ? pawn_sig : piece_sig;
      if (sig == 2)
        s += 220'000;  // gives check
      else if (sig == 1)
        s += 180'000;  // immediate threat

      // ---- Continuation History Contribution (layered) ----
      int chSum = 0;

      if (ply >= 1) {
        const model::Move pm1 = prevMove[cap_ply(ply - 1)];
        if (pm1.from() >= 0 && pm1.to() >= 0 && pm1.to() < 64) {
          if (auto po1 = board.getPiece(pm1.to())) {
            chSum += contHist[0][pidx(po1->type)][pm1.to()][pidx(moverPt)][m.to()];
          }
        }
      }

      if (ply >= 2) {
        const model::Move pm2 = prevMove[cap_ply(ply - 2)];
        if (pm2.from() >= 0 && pm2.to() >= 0 && pm2.to() < 64) {
          if (auto po2 = board.getPiece(pm2.to())) {
            chSum += contHist[1][pidx(po2->type)][pm2.to()][pidx(moverPt)][m.to()] >> 1;
          }
        }
      }

      if (ply >= 3) {
        const model::Move pm3 = prevMove[cap_ply(ply - 3)];
        if (pm3.from() >= 0 && pm3.to() >= 0 && pm3.to() < 64) {
          if (auto po3 = board.getPiece(pm3.to())) {
            chSum += contHist[2][pidx(po3->type)][pm3.to()][pidx(moverPt)][m.to()] >> 2;
          }
        }
      }

      // leichte Dämpfung, damit contHist nicht alles dominiert
      s += (chSum >> 1);
      // -----------------------------------------------------
    }

    scores[i] = s;
    ordered[i] = m;
  }
  sort_by_score_desc(scores, ordered, n);

  const bool allowFutility = !inCheck && !isPV;
  int moveCount = 0;

  for (int idx = 0; idx < n; ++idx) {
    if ((idx & 63) == 0) check_stop(stopFlag);

    const model::Move m = ordered[idx];
    if (excludedMove && m == *excludedMove) {
      ++moveCount;
      continue;
    }
    const bool isQuiet = !m.isCapture() && (m.promotion() == core::PieceType::None);
    const auto us = pos.getState().sideToMove;
    const int qp_sig = isQuiet ? quiet_pawn_push_signal(board, m, us) : 0;
    const int qpc_sig = isQuiet ? quiet_piece_threat_signal(board, m, us) : 0;

    // pre info
    auto moverOpt = board.getPiece(m.from());
    const core::PieceType moverPt = moverOpt ? moverOpt->type : core::PieceType::Pawn;
    core::PieceType capPt = core::PieceType::Pawn;

    // Nach moverPt-Bestimmung:
    const bool isQuietHeavy =
        isQuiet && (moverPt == core::PieceType::Queen || moverPt == core::PieceType::Rook);

    // Erweitere "tacticalQuiet":
    const bool tacticalQuiet = (qp_sig > 0) || (qpc_sig > 0);

    if (m.isEnPassant())
      capPt = core::PieceType::Pawn;
    else if (m.isCapture()) {
      if (auto cap = board.getPiece(m.to())) capPt = cap->type;
    }
    const int capValPre = m.isCapture() ? (m.isEnPassant() ? base_value[(int)core::PieceType::Pawn]
                                                           : base_value[(int)capPt])
                                        : 0;

    // LMP (contHist-aware)
    if (!inCheck && !isPV && isQuiet && depth <= 3 && !tacticalQuiet && !isQuietHeavy) {
      int hist = history[m.from()][m.to()] + (quietHist[pidx(moverPt)][m.to()] >> 1);

      int ch = 0;
      if (ply >= 1) {
        const auto pm1 = prevMove[cap_ply(ply - 1)];
        if (pm1.from() >= 0 && pm1.to() >= 0 && pm1.to() < 64) {
          if (auto po1 = board.getPiece(pm1.to()))
            ch = contHist[0][pidx(po1->type)][pm1.to()][pidx(moverPt)][m.to()];
        }
      }

      int limit = LMP_LIMIT[depth];
      if (hist < -8000) limit -= 1;
      if (ch < -8000) limit -= 1;
      if (limit < 1) limit = 1;

      int futMarg = FUT_MARGIN[depth] + (improving ? 32 : 0);
      if (staticEval + futMarg <= alpha + 32 && moveCount >= limit) {
        ++moveCount;
        continue;
      }
    }

    // Extended futility (depth<=3, quiets) — relax when improving
    if (allowFutility && isQuiet && depth <= 3 && !tacticalQuiet && !isQuietHeavy) {
      int fut = FUT_MARGIN[depth] + (history[m.from()][m.to()] < -8000 ? 32 : 0);
      if (improving) fut += 48;
      if (staticEval + fut <= alpha) {
        ++moveCount;
        continue;
      }
    }

    // History pruning — gate auf improving
    if (!inCheck && !isPV && isQuiet && depth <= 2 && !tacticalQuiet && !isQuietHeavy &&
        !improving) {
      int histScore = history[m.from()][m.to()] + (quietHist[pidx(moverPt)][m.to()] >> 1);
      if (histScore < -11000 && m != killers[kply][0] && m != killers[kply][1] &&
          (!prevOk || m != cm)) {
        ++moveCount;
        continue;
      }
    }

    // Futility (D1) — gate auf improving
    if (!inCheck && !isPV && isQuiet && depth == 1 && !tacticalQuiet && !isQuietHeavy &&
        !improving) {
      if (staticEval + 110 <= alpha) {
        ++moveCount;
        continue;
      }
    }

    // ---- SEE nur EINMAL bestimmen (falls nötig) ----
    bool seeGood = true;
    if (m.isCapture() && m.promotion() == core::PieceType::None) {
      const int attackerVal = base_value[(int)moverPt];
      const int victimVal = capValPre;  // bereits oben berechnet

      if (victimVal < attackerVal) {
        if (!inCheck && ply > 0 && depth <= 5) {
          // flach und nicht im Schach: fragwürdige Captures direkt kappen
          if (!pos.see(m)) {
            ++moveCount;
            continue;
          }
          seeGood = true;
        } else {
          // tiefer / kritischer: Ergebnis merken und später nutzen
          seeGood = pos.see(m);
        }
      }
    }

    const int mvvBefore =
        (m.isCapture() || m.promotion() != core::PieceType::None) ? mvv_lva_fast(pos, m) : 0;
    int newDepth = depth - 1;

    // ----- Singular Extension -----
    int seExt = 0;
    if (cfg.useSingularExt && haveTT && m == ttMove && !inCheck && depth >= 6) {
      const bool ttGood =
          (ttBound != model::Bound::Upper) && (ttStoredDepth >= depth - 2) && (ttVal > alpha + 8);
      if (ttGood && !is_mate_score(ttVal)) {
        const int R = (depth >= 8 ? 3 : 2);
        const int margin = 50 + 2 * depth;
        const int singBeta = ttVal - margin;

        if (singBeta > -MATE + 64) {
          model::Move dummy{};
          const int sDepth = std::max(1, depth - 1 - R);
          int s = negamax(pos, sDepth, singBeta - 1, singBeta, ply, dummy, staticEval, &m);
          if (s < singBeta) seExt = 1;
        }
      }
    }
    newDepth += seExt;

    MoveUndoGuard g(pos);
    if (!g.doMove(m)) {
      ++moveCount;
      continue;
    }

    prevMove[cap_ply(ply)] = m;
    tt.prefetch(pos.hash());

    int value;
    model::Move childBest{};

    // ProbCut (leicht erweitert)
    if (!isPV && !inCheck && newDepth >= 4 && m.isCapture() && seeGood && mvvBefore >= 500) {
      constexpr int PROBCUT_MARGIN = 224;
      if (staticEval + capValPre + PROBCUT_MARGIN >= beta) {
        const int red = 3;
        const int probe =
            -negamax(pos, newDepth - red, -beta, -(beta - 1), ply + 1, childBest, staticEval);
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
      value = -negamax(pos, newDepth, -beta, -alpha, ply + 1, childBest, staticEval);
    } else {
      if (cfg.useLMR && isQuiet && !tacticalQuiet && !inCheck && !givesCheck && newDepth >= 2 &&
          moveCount >= 3) {
        const int ld = ilog2_u32((unsigned)depth);
        const int lm = ilog2_u32((unsigned)(moveCount + 1));
        int r = (ld * (lm + 1)) / 2;  // kräftiger als /3
        if (isQuietHeavy) r = std::max(0, r - 1);

        const int h = history[m.from()][m.to()] + (quietHist[pidx(moverPt)][m.to()] >> 1);
        int ch = 0;
        if (ply >= 1) {
          const auto pm1 = prevMove[cap_ply(ply - 1)];
          if (pm1.from() >= 0 && pm1.to() >= 0 && pm1.to() < 64) {
            if (auto po1 = board.getPiece(pm1.to()))
              ch = contHist[0][pidx(po1->type)][pm1.to()][pidx(moverPt)][m.to()];
          }
        }

        if (h > 8000) r -= 1;
        if (ch > 8000) r -= 1;

        if (m == killers[kply][0] || m == killers[kply][1]) r -= 1;
        if (haveTT && m == ttMove) r -= 1;

        if (ply <= 2) r -= 1;
        if (beta - alpha <= 8) r -= 1;

        if (!improving) r += 1;

        if (r < 0) r = 0;
        int rCap = (newDepth >= 5 ? 3 : 2);
        if (r > rCap) r = rCap;
        reduction = std::min(r, newDepth - 1);
      }

      value =
          -negamax(pos, newDepth - reduction, -alpha - 1, -alpha, ply + 1, childBest, staticEval);
      if (value > alpha && value < beta) {
        value = -negamax(pos, newDepth, -beta, -alpha, ply + 1, childBest, staticEval);
      }
    }

    value = std::clamp(value, -MATE + 1, MATE - 1);

    // History updates
    if (isQuiet && value <= origAlpha) {
      const int M = hist_bonus(depth) / 2;

      hist_update(history[m.from()][m.to()], -M);
      hist_update(quietHist[pidx(moverPt)][m.to()], -M);

      // --- Continuation History: Malus ---
      if (ply >= 1) {
        const model::Move pm1 = prevMove[cap_ply(ply - 1)];
        if (pm1.from() >= 0 && pm1.to() >= 0 && pm1.to() < 64) {
          if (auto po1 = board.getPiece(pm1.to())) {
            hist_update(contHist[0][pidx(po1->type)][pm1.to()][pidx(moverPt)][m.to()], -M);
          }
        }
      }
      if (ply >= 2) {
        const model::Move pm2 = prevMove[cap_ply(ply - 2)];
        if (pm2.from() >= 0 && pm2.to() >= 0 && pm2.to() < 64) {
          if (auto po2 = board.getPiece(pm2.to())) {
            hist_update(contHist[1][pidx(po2->type)][pm2.to()][pidx(moverPt)][m.to()], -(M >> 1));
          }
        }
      }
      if (ply >= 3) {
        const model::Move pm3 = prevMove[cap_ply(ply - 3)];
        if (pm3.from() >= 0 && pm3.to() >= 0 && pm3.to() < 64) {
          if (auto po3 = board.getPiece(pm3.to())) {
            hist_update(contHist[2][pidx(po3->type)][pm3.to()][pidx(moverPt)][m.to()], -(M >> 2));
          }
        }
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

        const int B = hist_bonus(depth);

        hist_update(history[m.from()][m.to()], +B);
        hist_update(quietHist[pidx(moverPt)][m.to()], +B);

        // --- Continuation History: Bonus ---
        if (ply >= 1) {
          const model::Move pm1 = prevMove[cap_ply(ply - 1)];
          if (pm1.from() >= 0 && pm1.to() >= 0 && pm1.to() < 64) {
            if (auto po1 = board.getPiece(pm1.to())) {
              hist_update(contHist[0][pidx(po1->type)][pm1.to()][pidx(moverPt)][m.to()], +B);
            }
          }
        }
        if (ply >= 2) {
          const model::Move pm2 = prevMove[cap_ply(ply - 2)];
          if (pm2.from() >= 0 && pm2.to() >= 0 && pm2.to() < 64) {
            if (auto po2 = board.getPiece(pm2.to())) {
              hist_update(contHist[1][pidx(po2->type)][pm2.to()][pidx(moverPt)][m.to()], +(B >> 1));
            }
          }
        }
        if (ply >= 3) {
          const model::Move pm3 = prevMove[cap_ply(ply - 3)];
          if (pm3.from() >= 0 && pm3.to() >= 0 && pm3.to() < 64) {
            if (auto po3 = board.getPiece(pm3.to())) {
              hist_update(contHist[2][pidx(po3->type)][pm3.to()][pidx(moverPt)][m.to()], +(B >> 2));
            }
          }
        }

        if (prevOk) {
          counterMove[prev.from()][prev.to()] = m;
          hist_update(counterHist[prev.from()][prev.to()], +B);
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

    if (m.from() == m.to() || tte.bound != model::Bound::Exact) break;
    if (!pos.doMove(m)) break;
    pv.push_back(m);

    uint64_t h = pos.hash();
    if (!seen.insert(h).second) break;  // TT-Loop erkannt
  }
  return pv;
}

// ---------- Root Search (parallel, Work-Queue, with heuristic copy/merge) ----------
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
    stats.nodes = sharedNodeCounter->load(std::memory_order_relaxed);
    update_time_stats();
    this->stopFlag.reset();

    const int score = pos.inCheck() ? mated_in(0) : 0;  // mate vs stalemate
    stats.bestScore = score;
    stats.bestPV.clear();
    stats.topMoves.clear();
    return score;
  }

  // Thread-Pool setup
  auto& pool = ThreadPool::instance();
  const int threads = std::max(1, maxThreads > 0 ? std::min(maxThreads, cfg.threads) : cfg.threads);

  // --- Persistent Workers ---
  std::vector<std::unique_ptr<Search>> workers;
  workers.reserve(threads);
  for (int t = 0; t < threads; ++t) {
    auto w = std::make_unique<Search>(tt, eval_, cfg);
    w->stopFlag = stop;
    w->set_node_limit(sharedNodeCounter, maxNodes);
    workers.emplace_back(std::move(w));
  }

  // Aspiration seed
  int lastScoreGuess = 0;
  if (cfg.useAspiration) {
    model::TTEntry5 tte{};
    if (tt.probe_into(pos.hash(), tte)) lastScoreGuess = decode_tt_score(tte.value, 0);
  }

  auto score_root_move = [&](const model::Move& m, const model::Move& ttMove, bool haveTT) {
    int s = 0;

    const bool isPromo = (m.promotion() != core::PieceType::None);
    const bool isCap = m.isCapture();

    // Strong TT bias
    if (haveTT && m == ttMove) s += 2'500'000;

    if (isPromo) {
      s += 1'200'000;
    } else if (isCap) {
      int mvvlva = mvv_lva_fast(pos, m);
      if (!pos.see(m)) mvvlva -= 350;
      s += 1'050'000 + mvvlva;
    } else {
      int h = history[m.from()][m.to()];
      if (h > 20'000) h = 20'000;
      if (h < -20'000) h = -20'000;
      s += h;

      auto mover = pos.getBoard().getPiece(m.from());
      if (mover) {
        const int pawn_sig =
            (mover->type == core::PieceType::Pawn)
                ? quiet_pawn_push_signal(pos.getBoard(), m, pos.getState().sideToMove)
                : 0;
        const int piece_sig =
            quiet_piece_threat_signal(pos.getBoard(), m, pos.getState().sideToMove);
        const int sig = pawn_sig > piece_sig ? pawn_sig : piece_sig;
        if (sig == 2)
          s += 12'000;
        else if (sig == 1)
          s += 8'000;
      }
    }

    return s;
  };

  struct RootResult {
    std::atomic<int> nullScore{std::numeric_limits<int>::min()};
    std::atomic<int> fullScore{std::numeric_limits<int>::min()};
  };

  // NEW: mutex to merge thread-local heuristics back to coordinator
  std::mutex heurMergeMx;

  for (int depth = 1; depth <= std::max(1, maxDepth); ++depth) {
    if (stop && stop->load()) break;

    // Mild decay
    if (depth > 1) {
      decay_tables(*this, /*shift=*/6);
    }
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

        Search& worker = *workers[0];
        worker.copy_heuristics_from(*this);  // seed einmal je Tiefe
        worker.prevMove[0] = m0;

        model::Move ref{};
        int s = 0;
        try {
          if (!cfg.useAspiration || depth - 1 < 3 || is_mate_score(lastScoreGuess)) {
            s = -worker.negamax(child, depth - 1, -INF, INF, 1, ref, INF);
          } else {
            int w = std::max(12, cfg.aspirationWindow);
            int low = lastScoreGuess - w, high = lastScoreGuess + w;
            for (int tries = 0; tries < 3; ++tries) {
              s = -worker.negamax(child, depth - 1, -high, -low, 1, ref, INF);
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
              s = -worker.negamax(child, depth - 1, -INF, INF, 1, ref, INF);
            }
          }
        } catch (const SearchStoppedException&) {
          if (stop) stop->store(true, std::memory_order_relaxed);
          stats.nodes = sharedNodeCounter->load(std::memory_order_relaxed);
          update_time_stats();
          int fallback = sharedAlpha.load(std::memory_order_relaxed);
          if (fallback == -INF) fallback = 0;
          stats.bestScore = std::clamp(fallback, -MATE + 1, MATE - 1);
          this->stopFlag.reset();
          return stats.bestScore;
        }

        s = std::clamp(s, -MATE + 1, MATE - 1);
        results[0].nullScore.store(s, std::memory_order_relaxed);
        results[0].fullScore.store(s, std::memory_order_relaxed);
        sharedAlpha.store(s, std::memory_order_relaxed);

        // NEW: merge this worker's learned heuristics back
        {
          std::scoped_lock lk(heurMergeMx);
          this->merge_from(worker);
        }
      }
    }

    // 2) Restliche Root-Moves per Work-Queue
    const int total = (int)rootMoves.size();
    std::atomic<int> idx{1};

    auto workerFn = [&](int tid) {
      // tid ist der Index im Submit-Loop; nutze workers[tid + 1]
      Search& worker = *workers[tid + 1];
      worker.copy_heuristics_from(*this);  // seed je Tiefe

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

          int sN = -worker.negamax(child, depth - 1, -(localAlpha + 1), -localAlpha, 1, ref, INF);
          sN = std::clamp(sN, -MATE + 1, MATE - 1);

          results[i].nullScore.store(sN, std::memory_order_relaxed);

          // re-read α und nur "near-α" verifizieren
          int alphaNow = sharedAlpha.load(std::memory_order_relaxed);
          int verifyMargin = ROOT_VERIFY_MARGIN_BASE + (depth >= 10 ? 8 : 0);
          int dropMargin = 96 + (depth >= 10 ? 8 : 0);

          if (sN >= alphaNow - verifyMargin && !(stop && stop->load())) {
            int sF = -worker.negamax(child, depth - 1, -INF, INF, 1, ref, INF);
            sF = std::clamp(sF, -MATE + 1, MATE - 1);

            results[i].fullScore.store(sF, std::memory_order_relaxed);

            int cur = alphaNow;
            while (sF > cur &&
                   !sharedAlpha.compare_exchange_weak(cur, sF, std::memory_order_relaxed)) {
            }
          } else if (sN < alphaNow - dropMargin) {
            // klar unter α -> kein Full-Window nötig
            continue;
          }
        }
      } catch (const SearchStoppedException&) {
        if (stop) stop->store(true, std::memory_order_relaxed);
      }

      // NEW: merge once per worker (coarse, low contention)
      {
        std::scoped_lock lk(heurMergeMx);
        this->merge_from(worker);
      }
    };

    const int numWorkers = std::min(total - 1, threads - 1);
    std::vector<std::future<void>> futs;
    futs.reserve(numWorkers);
    for (int t = 0; t < numWorkers; ++t)
      futs.emplace_back(pool.submit([&, tid = t] { workerFn(tid); }));

    for (auto& f : futs) {
      try {
        f.get();
      } catch (...) {
      }
    }

    // --- Reuse persistent worker[0] als Fallback (sequenziell) ---
    Search& fbWorker = *workers[0];
    fbWorker.copy_heuristics_from(*this);

    for (int i = 1; i < total; ++i) {
      if (stop && stop->load()) break;
      if (results[i].nullScore.load(std::memory_order_relaxed) != std::numeric_limits<int>::min())
        continue;  // already processed by a worker

      model::Position child = pos;
      if (!child.doMove(rootMoves[i])) continue;

      tt.prefetch(child.hash());
      fbWorker.prevMove[0] = rootMoves[i];

      model::Move ref{};
      try {
        int localAlpha = sharedAlpha.load(std::memory_order_relaxed);

        int sN = -fbWorker.negamax(child, depth - 1, -(localAlpha + 1), -localAlpha, 1, ref, INF);
        sN = std::clamp(sN, -MATE + 1, MATE - 1);
        results[i].nullScore.store(sN, std::memory_order_relaxed);

        int alphaNow = sharedAlpha.load(std::memory_order_relaxed);
        int verifyMargin = ROOT_VERIFY_MARGIN_BASE + (depth >= 10 ? 8 : 0);
        int dropMargin = 96 + (depth >= 10 ? 8 : 0);

        if (sN >= alphaNow - verifyMargin && !(stop && stop->load())) {
          int sF = -fbWorker.negamax(child, depth - 1, -INF, INF, 1, ref, INF);
          sF = std::clamp(sF, -MATE + 1, MATE - 1);
          results[i].fullScore.store(sF, std::memory_order_relaxed);

          int cur = alphaNow;
          while (sF > cur &&
                 !sharedAlpha.compare_exchange_weak(cur, sF, std::memory_order_relaxed)) {
          }
        } else if (sN < alphaNow - dropMargin) {
          continue;
        }
      } catch (const SearchStoppedException&) {
        if (stop) stop->store(true, std::memory_order_relaxed);
        break;
      }
    }

    // Merge fallback worker once
    {
      std::scoped_lock lk(heurMergeMx);
      this->merge_from(fbWorker);
    }

    // 3) Confirm-Pass (guarded)
    try {
      const int bestAlphaNow = sharedAlpha.load(std::memory_order_relaxed);

      Search& confirmWorker = *workers[1 < threads ? 1 : 0];
      confirmWorker.copy_heuristics_from(*this);  // seed je Tiefe

      for (int i = 1; i < total; ++i) {
        if (stop && stop->load()) break;
        const int sN = results[i].nullScore.load(std::memory_order_relaxed);
        const int sF = results[i].fullScore.load(std::memory_order_relaxed);
        if (sF != std::numeric_limits<int>::min()) continue;

        int verifyMargin = ROOT_VERIFY_MARGIN_BASE + (depth >= 10 ? 8 : 0);
        if (sN < bestAlphaNow - verifyMargin) continue;

        model::Position child = pos;
        if (!child.doMove(rootMoves[i])) continue;

        confirmWorker.prevMove[0] = rootMoves[i];

        model::Move ref{};
        int sF2 = -confirmWorker.negamax(child, depth - 1, -INF, INF, 1, ref, INF);
        sF2 = std::clamp(sF2, -MATE + 1, MATE - 1);
        results[i].fullScore.store(sF2, std::memory_order_relaxed);

        int cur = sharedAlpha.load(std::memory_order_relaxed);
        while (sF2 > cur &&
               !sharedAlpha.compare_exchange_weak(cur, sF2, std::memory_order_relaxed)) {
        }
      }

      // merge confirm worker’s heuristics once
      {
        std::scoped_lock lk(heurMergeMx);
        this->merge_from(confirmWorker);
      }
    } catch (const SearchStoppedException&) {
      if (stop) stop->store(true, std::memory_order_relaxed);
      // fall through to ranking with what we have
    }

    // 4) Ranking
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

    bool anyScored = std::any_of(depthCand.begin(), depthCand.end(), [](const auto& p) {
      return p.first != std::numeric_limits<int>::min();
    });
    if (!anyScored) break;

    int NDISPLAY = (depth >= 6 ? 2 : 1);
    NDISPLAY = std::min<int>(NDISPLAY, (int)depthCand.size());
    std::partial_sort(depthCand.begin(), depthCand.begin() + std::max(1, NDISPLAY), depthCand.end(),
                      [](const auto& a, const auto& b) { return a.first > b.first; });
    stats.nodes = sharedNodeCounter->load(std::memory_order_relaxed);
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

    const int SHOW = std::min<int>(5, (int)depthCand.size());
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
  std::memset(contHist, 0, sizeof(contHist));
  for (auto& row : counterMove)
    for (auto& m : row) m = model::Move{};
  for (auto& pm : prevMove) pm = model::Move{};
  stats = SearchStats{};
}

void Search::copy_heuristics_from(const Search& src) {
  // History-like tables
  history = src.history;  // std::array copy

  std::memcpy(quietHist, src.quietHist, sizeof(quietHist));
  std::memcpy(captureHist, src.captureHist, sizeof(captureHist));
  std::memcpy(counterHist, src.counterHist, sizeof(counterHist));
  std::memcpy(counterMove, src.counterMove, sizeof(counterMove));

  // Killers nicht kopieren
  for (auto& kk : killers) {
    kk[0] = model::Move{};
    kk[1] = model::Move{};
  }

  // prevMove buffers sind lokaler Pfad-State
  for (auto& pm : prevMove) pm = model::Move{};
}

// EMA merge toward the worker's values: G += (L - G) / K
static inline int16_t ema_merge(int16_t G, int16_t L, int K) {
  int d = (int)L - (int)G;
  return clamp16((int)G + d / K);
}

void Search::merge_from(const Search& o) {
  // kleiner K -> schnelleres Lernen
  constexpr int K = 4;

  // Base history
  for (int f = 0; f < SQ_NB; ++f)
    for (int t = 0; t < SQ_NB; ++t) history[f][t] = ema_merge(history[f][t], o.history[f][t], K);

  // Quiet history
  for (int p = 0; p < PIECE_NB; ++p)
    for (int t = 0; t < SQ_NB; ++t)
      quietHist[p][t] = ema_merge(quietHist[p][t], o.quietHist[p][t], K);

  // Capture history
  for (int mp = 0; mp < PIECE_NB; ++mp)
    for (int t = 0; t < SQ_NB; ++t)
      for (int cp = 0; cp < PIECE_NB; ++cp)
        captureHist[mp][t][cp] = ema_merge(captureHist[mp][t][cp], o.captureHist[mp][t][cp], K);

  // Counter history + best countermove choice
  for (int f = 0; f < SQ_NB; ++f) {
    for (int t = 0; t < SQ_NB; ++t) {
      counterHist[f][t] = ema_merge(counterHist[f][t], o.counterHist[f][t], K);
      if (o.counterHist[f][t] > counterHist[f][t]) {
        counterMove[f][t] = o.counterMove[f][t];
      }
    }
  }

  // Continuation History (EMA)
  for (int L = 0; L < CH_LAYERS; ++L)
    for (int pp = 0; pp < PIECE_NB; ++pp)
      for (int pt = 0; pt < SQ_NB; ++pt)
        for (int mp = 0; mp < PIECE_NB; ++mp)
          for (int to = 0; to < SQ_NB; ++to)
            contHist[L][pp][pt][mp][to] =
                ema_merge(contHist[L][pp][pt][mp][to], o.contHist[L][pp][pt][mp][to], K);
}

}  // namespace lilia::engine
