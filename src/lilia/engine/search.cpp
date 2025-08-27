#pragma once

#include "lilia/engine/search.hpp"

#include <algorithm>
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

// ---------- Mate-(De)Normalisierung ----------
static constexpr int MATE_THR = MATE - 512;

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

// ---------- Guards / Helpers ----------
namespace {

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

// --- kleine Int-Tools / History-NL-Update ---
static inline int ilog2_u32(unsigned v) {
  int r = 0;
  while (v >>= 1) ++r;
  return r;
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

}  // namespace

// ---------- Search ----------
Search::Search(model::TT4& tt_, std::shared_ptr<const Evaluator> eval_, const EngineConfig& cfg_)
    : tt(tt_), mg(), cfg(cfg_), eval_(std::move(eval_)) {
  for (auto& kk : killers) {
    kk[0] = model::Move{};
    kk[1] = model::Move{};
  }
  for (auto& h : history) h.fill(0);
  for (auto& v : genBuf_) v.reserve(96);
  for (auto& v : legalBuf_) v.reserve(96);
  qAllBuf_.reserve(96);
  qMovesBuf_.reserve(96);
  stopFlag.reset();
  stats = SearchStats{};
}

int Search::signed_eval(model::Position& pos) {
  int v = eval_->evaluate(pos);
  if (pos.getState().sideToMove == core::Color::Black) v = -v;
  return std::clamp(v, -MATE + 1, MATE - 1);
}

// ---------- Quiescence ----------
int Search::quiescence(model::Position& pos, int alpha, int beta, int ply) {
  stats.nodes++;

  if (ply >= MAX_PLY - 2) return signed_eval(pos);
  if (stopFlag && stopFlag->load()) return signed_eval(pos);

  const int kply = cap_ply(ply);

  if (pos.inCheck()) {
    int best = -INF;
    auto& moves = genBuf_[kply];  // Reuse-Buffer statt lokaler Vector
    if (!safeGenerateMoves(mg, pos, moves)) return mated_in(ply);

    bool anyLegal = false;
    for (auto& m : moves) {
      MoveUndoGuard g(pos);
      if (!g.doMove(m)) continue;
      anyLegal = true;

      tt.prefetch(pos.hash());  // Prefetch child TT
      int score = -quiescence(pos, -beta, -alpha, ply + 1);
      score = std::clamp(score, -MATE + 1, MATE - 1);
      g.rollback();

      if (score >= beta) return beta;
      if (score > best) best = score;
      if (score > alpha) alpha = score;
    }
    if (!anyLegal) return mated_in(ply);
    return best;
  }

  int stand = signed_eval(pos);
  if (stand >= beta) return beta;
  if (alpha < stand) alpha = stand;

  auto& all = qAllBuf_;
  auto& qmoves = qMovesBuf_;
  if (!safeGenerateMoves(mg, pos, all)) return stand;

  qmoves.clear();
  qmoves.reserve(all.size());
  for (auto& m : all) {
    if (m.isCapture || m.promotion != core::PieceType::None) qmoves.push_back(m);
  }

  std::sort(qmoves.begin(), qmoves.end(), [&pos](const model::Move& a, const model::Move& b) {
    return mvv_lva_score(pos, a) > mvv_lva_score(pos, b);
  });

  constexpr int DELTA_MARGIN = 80;
  int best = stand;

  for (auto& m : qmoves) {
    if (stopFlag && stopFlag->load()) break;

    if (m.isCapture && !pos.see(m) && m.promotion == core::PieceType::None) continue;

    const bool isQuietPromo = (m.promotion != core::PieceType::None) && !m.isCapture;

    int capVal = 0;
    if (m.isEnPassant) {
      capVal = base_value[(int)core::PieceType::Pawn];
    } else if (m.isCapture) {
      if (auto cap = pos.getBoard().getPiece(m.to)) capVal = base_value[(int)cap->type];
    }

    int promoGain = 0;
    if (m.promotion != core::PieceType::None) {
      promoGain =
          std::max(0, base_value[(int)m.promotion] - base_value[(int)core::PieceType::Pawn]);
    }

    if (!isQuietPromo) {
      if (stand + capVal + promoGain + DELTA_MARGIN <= alpha) continue;
    }

    MoveUndoGuard g(pos);
    if (!g.doMove(m)) continue;

    tt.prefetch(pos.hash());  // Prefetch child TT
    int score = -quiescence(pos, -beta, -alpha, ply + 1);
    score = std::clamp(score, -MATE + 1, MATE - 1);
    g.rollback();

    if (score >= beta) return beta;
    if (score > alpha) alpha = score;
    if (score > best) best = score;
  }

  return best;
}

// ---------- Negamax ----------
int Search::negamax(model::Position& pos, int depth, int alpha, int beta, int ply,
                    model::Move& refBest) {
  stats.nodes++;

  if (ply >= MAX_PLY - 2) return signed_eval(pos);
  check_stop(stopFlag);

  if (pos.checkInsufficientMaterial() || pos.checkMoveRule() || pos.checkRepetition()) return 0;
  if (depth <= 0) return quiescence(pos, alpha, beta, ply);

  // Mate Distance Pruning
  alpha = std::max(alpha, mated_in(ply));
  beta = std::min(beta, mate_in(ply));
  if (alpha >= beta) return alpha;

  const int origAlpha = alpha;
  const int origBeta = beta;

  const bool inCheck = pos.inCheck();
  const int staticEval = inCheck ? 0 : signed_eval(pos);

  // konservative Reverse-Futility für kleine Tiefen
  if (!inCheck && depth <= 2) {
    const int margin = (depth == 1 ? 170 : 260);
    if (staticEval - margin >= beta) return staticEval;
  }

  int best = -INF;
  model::Move bestLocal{};

  // --- TT-Probe (3 Zeilen umgeschrieben: probe_into) ---
  model::Move ttMove{};
  bool haveTT = false;
  {
    model::TTEntry4 tte{};
    if (tt.probe_into(pos.hash(), tte)) {
      haveTT = true;
      ttMove = tte.best;

      MoveUndoGuard g(pos);
      bool ok = (ttMove.from >= 0 && ttMove.to >= 0 && g.doMove(ttMove));
      g.rollback();
      if (!ok) haveTT = false;

      const int ttVal = decode_tt_score(tte.value, cap_ply(ply));
      if (haveTT && tte.depth >= depth) {
        if (tte.bound == model::Bound::Exact) return std::clamp(ttVal, -MATE + 1, MATE - 1);
        if (tte.bound == model::Bound::Lower) alpha = std::max(alpha, ttVal);
        if (tte.bound == model::Bound::Upper) beta = std::min(beta, ttVal);
        if (alpha >= beta) return std::clamp(ttVal, -MATE + 1, MATE - 1);
      }
    }
  }

  // IID (sanft)
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
      g.rollback();
    }
  }

  // Null-Move (konservativ)
  auto nonPawnCount = [&](const model::Board& b) {
    using PT = core::PieceType;
    auto countSide = [&](core::Color c) {
      return model::bb::popcount(b.getPieces(c, PT::Knight) | b.getPieces(c, PT::Bishop) |
                                 b.getPieces(c, PT::Rook) | b.getPieces(c, PT::Queen));
    };
    return countSide(core::Color::White) + countSide(core::Color::Black);
  };
  const bool sparse = (nonPawnCount(pos.getBoard()) <= 3);

  if (cfg.useNullMove && depth >= 3 && !inCheck && !sparse) {
    NullUndoGuard ng(pos);
    ng.doNull();
    if (ng.applied) {
      int R = 2 + (depth >= 6 ? 1 : 0);
      if (!inCheck && staticEval >= beta) R++;
      int nullScore = -negamax(pos, depth - 1 - R, -beta, -beta + 1, ply + 1, refBest);
      ng.rollback();
      if (nullScore >= beta) return beta;
    }
  }

  const int kply = cap_ply(ply);
  auto& gen = genBuf_[kply];
  auto& legal = legalBuf_[kply];

  gen.clear();
  if (!safeGenerateMoves(mg, pos, gen)) {
    if (inCheck) return mated_in(ply);
    return 0;
  }

  legal.clear();
  legal.reserve(gen.size());
  for (auto& m : gen) {
    MoveUndoGuard g(pos);
    if (!g.doMove(m)) continue;
    g.rollback();
    legal.push_back(m);
  }

  if (legal.empty()) {
    if (inCheck) return mated_in(ply);
    return 0;
  }

  // Move Ordering
  constexpr int MAX_MOVES = 256;
  int n = (int)legal.size();
  if (n > MAX_MOVES) n = MAX_MOVES;

  int scores[MAX_MOVES];
  model::Move ordered[MAX_MOVES];

  constexpr int TT_BONUS = 1'000'000;
  constexpr int GOOD_CAP = 100'000;
  constexpr int PROMO_BASE = 80'000;
  constexpr int KILLER_BASE = 60'000;
  constexpr int BAD_CAP = -50'000;

  for (int i = 0; i < n; ++i) {
    const auto& m = legal[i];
    int s = 0;

    if (haveTT && ttMove == m) {
      s = TT_BONUS;
    } else if (m.isCapture || m.promotion != core::PieceType::None) {
      const int mvv = mvv_lva_score(pos, m);
      const bool good = !m.isCapture || pos.see(m);
      if (m.promotion != core::PieceType::None && !m.isCapture)
        s = PROMO_BASE + mvv;
      else if (good)
        s = GOOD_CAP + mvv;
      else
        s = BAD_CAP + mvv;
    } else if (m == killers[kply][0] || m == killers[kply][1]) {
      s = KILLER_BASE;
    } else {
      s = history[m.from][m.to];
    }

    scores[i] = s;
    ordered[i] = m;
  }

  sort_by_score_desc(scores, ordered, n);

  // PVS + LMR + LMP + Futility + SEE-Pruning + TT-Prefetch
  int moveCount = 0;
  for (int idx = 0; idx < n; ++idx) {
    model::Move m = ordered[idx];
    if (stopFlag && stopFlag->load()) break;

    const bool isQuiet = !m.isCapture && (m.promotion == core::PieceType::None);

    // SEE-Pruning für schlechte Captures
    if (!inCheck && m.isCapture && depth <= 5) {
      if (!pos.see(m)) {
        ++moveCount;
        continue;
      }
    }

    // LMP (nur Quiet)
    if (!inCheck && isQuiet && depth <= 3) {
      const int lmpLimit = 2 + depth * depth;
      if (moveCount >= lmpLimit) {
        ++moveCount;
        continue;
      }
    }

    // shallow futility (nur Quiet)
    if (!inCheck && depth == 1 && isQuiet) {
      if (staticEval + 125 <= alpha) {
        ++moveCount;
        continue;
      }
    }

    MoveUndoGuard g(pos);
    if (!g.doMove(m)) {
      ++moveCount;
      continue;
    }

    // Prefetch TT für Kind
    tt.prefetch(pos.hash());

    int value;
    model::Move childBest{};
    int newDepth = depth - 1;

    // Check-Extension
    const bool givesCheck = pos.inCheck();
    if (givesCheck) newDepth += 1;

    if (moveCount == 0) {
      // PVS: voller Plan für den ersten
      value = -negamax(pos, newDepth, -beta, -alpha, ply + 1, childBest);
    } else {
      // --- LMR: history-aware, konservativ geklammert ---
      int reduction = 0;
      if (cfg.useLMR && isQuiet && !inCheck && !givesCheck && depth >= 3) {
        const int ld = ilog2_u32((unsigned)depth);
        const int lm = ilog2_u32((unsigned)(moveCount + 1));
        int rBase = (ld * (lm + 1)) / 2;

        const int h = history[m.from][m.to];
        if (h > 6000)
          rBase -= 1;
        else if (h < -6000)
          rBase += 1;

        if (m == killers[kply][0] || m == killers[kply][1]) rBase -= 1;
        if (haveTT && m == ttMove) rBase -= 1;

        if (rBase < 0) rBase = 0;
        if (rBase > 3) rBase = 3;
        reduction = std::min(rBase, newDepth - 1);
      }

      // Nullfenster
      value = -negamax(pos, newDepth - reduction, -alpha - 1, -alpha, ply + 1, childBest);
      // interessant? -> Vollfenster
      if (value > alpha && value < beta) {
        value = -negamax(pos, newDepth, -beta, -alpha, ply + 1, childBest);
      }
    }

    value = std::clamp(value, -MATE + 1, MATE - 1);
    g.rollback();

    if (isQuiet) {
      if (value <= alpha) {
        hist_update(history[m.from][m.to], -hist_bonus(depth) / 2);
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
      }
      break;
    }
    ++moveCount;
  }

  if (bestLocal.from < 0 && !legal.empty()) bestLocal = legal.front();

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

    bool isPawn = (pieceOpt->type == core::PieceType::Pawn);
    int toRank = ((int)m.to) >> 3;
    bool onPromoRank = (pieceOpt->color == core::Color::White) ? (toRank == 7) : (toRank == 0);
    if (isPawn && onPromoRank && m.promotion == core::PieceType::None) break;

    if (!pos.doMove(m)) break;
    pv.push_back(m);
  }
  return pv;
}

int Search::search_root_parallel(model::Position& pos, int maxDepth,
                                 std::shared_ptr<std::atomic<bool>> stop, int maxThreads) {
  this->stopFlag = stop;
  stats = SearchStats{};

  try {
    tt.new_generation();
  } catch (...) {
  }

  auto startAll = steady_clock::now();

  // Root-Moves generieren & legalisieren
  std::vector<model::Move> baseGen;
  if (!safeGenerateMoves(mg, pos, baseGen)) {
    this->stopFlag.reset();
    return 0;
  }
  std::vector<model::Move> rootMoves;
  rootMoves.reserve(baseGen.size());
  for (auto& m : baseGen) {
    MoveUndoGuard g(pos);
    if (!g.doMove(m)) continue;
    g.rollback();
    rootMoves.push_back(m);
  }
  if (rootMoves.empty()) {
    this->stopFlag.reset();
    return 0;
  }

  // Thread-Pool
  auto& pool = ThreadPool::instance(maxThreads);
  pool.maybe_resize(maxThreads > 0 ? maxThreads : (int)std::thread::hardware_concurrency());

  int stableBestScore = -INF;
  model::Move stableBestMove = rootMoves.front();

  int lastScoreGuess = 0;
  if (cfg.useAspiration) {
    model::TTEntry4 tte{};
    if (tt.probe_into(pos.hash(), tte)) lastScoreGuess = decode_tt_score(tte.value, 0);
  }

  for (int depth = 1; depth <= std::max(1, maxDepth); ++depth) {
    if (stop && stop->load()) break;

    // Root-Ordering (TT, Captures MVV/LVA, Promos)
    model::Move rootTT{};
    bool haveRootTT = false;
    {
      model::TTEntry4 tte{};
      if (tt.probe_into(pos.hash(), tte)) {
        haveRootTT = true;
        rootTT = tte.best;
      }
    }

    std::sort(rootMoves.begin(), rootMoves.end(), [&](const model::Move& a, const model::Move& b) {
      auto score = [&](const model::Move& m) {
        if (haveRootTT && m == rootTT) return 2'000'000;
        if (m.isCapture) return 1'000'000 + mvv_lva_score(pos, m);
        if (m.promotion != core::PieceType::None) return 900'000;
        return 0;
      };
      return score(a) > score(b);
    });

    struct RootResult {
      int nullScore = -INF;
      int fullScore = -INF;
      model::Move move{};
      SearchStats stats{};
    };

    std::atomic<int> sharedAlpha{-INF};
    std::vector<std::future<RootResult>> futures;
    futures.reserve(rootMoves.size());

    // Erstes Kind synchron (Aspiration → Vollfenster)
    RootResult firstRR{};
    try {
      model::Move m0 = rootMoves.front();
      model::Position child = pos;
      if (child.doMove(m0)) {
        tt.prefetch(child.hash());  // Prefetch TT für Kind
        Search worker(tt, eval_, cfg);
        worker.stopFlag = stop;
        model::Move ref{};
        int a = -INF, b = INF;
        if (cfg.useAspiration && depth >= 4) {
          const int w = std::max(10, cfg.aspirationWindow);
          a = lastScoreGuess - w;
          b = lastScoreGuess + w;
        }
        int s = (cfg.useAspiration && depth >= 4)
                    ? -worker.negamax(child, depth - 1, -b, -a, 1, ref)
                    : -worker.negamax(child, depth - 1, -INF, INF, 1, ref);
        if (cfg.useAspiration && depth >= 4 && (s <= a || s >= b))
          s = -worker.negamax(child, depth - 1, -INF, INF, 1, ref);
        s = std::clamp(s, -MATE + 1, MATE - 1);

        firstRR.move = m0;
        firstRR.nullScore = s;
        firstRR.fullScore = s;
        sharedAlpha.store(s, std::memory_order_relaxed);
        stats.nodes += worker.getStats().nodes;
      }
    } catch (...) {
    }

    // restliche Kinder über Thread-Pool
    for (size_t i = 1; i < rootMoves.size(); ++i) {
      if (stop && stop->load()) break;
      model::Move m = rootMoves[i];
      model::Position child = pos;
      if (!child.doMove(m)) continue;

      futures.emplace_back(
          pool.submit([this, stop, depth, m, child = std::move(child), &sharedAlpha]() mutable {
            RootResult rr{};
            rr.move = m;
            try {
              tt.prefetch(child.hash());  // Prefetch TT für Kind
              Search worker(tt, eval_, cfg);
              worker.stopFlag = stop;
              model::Move ref{};
              int localAlpha = sharedAlpha.load(std::memory_order_relaxed);

              int sN = -worker.negamax(child, depth - 1, -(localAlpha + 1), -localAlpha, 1, ref);
              sN = std::clamp(sN, -MATE + 1, MATE - 1);
              rr.nullScore = sN;

              if (sN >= localAlpha) {
                int sF = -worker.negamax(child, depth - 1, -INF, INF, 1, ref);
                sF = std::clamp(sF, -MATE + 1, MATE - 1);
                rr.fullScore = sF;

                int cur = localAlpha;
                while (sF > cur &&
                       !sharedAlpha.compare_exchange_weak(cur, sF, std::memory_order_relaxed)) {
                }
              }
              rr.stats = worker.getStats();
            } catch (...) {
            }
            return rr;
          }));
    }

    // Einsammeln
    std::vector<RootResult> completed;
    completed.reserve(futures.size() + 1);
    for (auto& f : futures) {
      try {
        completed.push_back(f.get());
      } catch (...) {
      }
    }
    completed.push_back(firstRR);

    // Confirm-Pass
    {
      const int bestAlpha = sharedAlpha.load(std::memory_order_relaxed);
      for (auto& rr : completed) {
        if (rr.fullScore == -INF && rr.nullScore >= bestAlpha) {
          model::Position child = pos;
          if (child.doMove(rr.move)) {
            tt.prefetch(child.hash());  // Prefetch TT für Kind
            Search worker(tt, eval_, cfg);
            worker.stopFlag = stop;
            model::Move ref{};
            int sF = -worker.negamax(child, depth - 1, -INF, INF, 1, ref);
            sF = std::clamp(sF, -MATE + 1, MATE - 1);
            rr.fullScore = sF;
            stats.nodes += worker.getStats().nodes;
          }
        }
        stats.nodes += rr.stats.nodes;
      }
    }

    // Ranking
    std::vector<std::pair<int, model::Move>> depthCand;
    depthCand.reserve(completed.size());
    for (auto& rr : completed) {
      int s = (rr.fullScore != -INF ? rr.fullScore : rr.nullScore);
      depthCand.emplace_back(s, rr.move);
    }
    std::sort(depthCand.begin(), depthCand.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    // Exakte Top-N (klein halten)
    const int NDISPLAY = std::min<int>(5, (int)depthCand.size());
    for (int i = 0; i < NDISPLAY; ++i) {
      const auto m = depthCand[i].second;
      model::Position child = pos;
      if (child.doMove(m)) {
        tt.prefetch(child.hash());  // Prefetch TT für Kind
        Search worker(tt, eval_, cfg);
        worker.stopFlag = stop;
        model::Move ref{};
        int s = -worker.negamax(child, depth - 1, -INF, INF, 1, ref);
        s = std::clamp(s, -MATE + 1, MATE - 1);
        depthCand[i].first = s;
        stats.nodes += worker.getStats().nodes;
      }
    }
    std::sort(depthCand.begin(), depthCand.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    int bestScoreThisDepth = -INF;
    model::Move bestMoveThisDepth = rootMoves.front();
    if (!depthCand.empty()) {
      bestScoreThisDepth = depthCand.front().first;
      bestMoveThisDepth = depthCand.front().second;
    }
    lastScoreGuess = bestScoreThisDepth;

    auto nowAll = steady_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(nowAll - startAll).count();
    stats.elapsedMs = ms;
    stats.nps = (ms > 0) ? (double)stats.nodes / (ms / 1000.0) : (double)stats.nodes;

    // PV/TopMoves
    stats.bestPV.clear();
    stats.bestMove = bestMoveThisDepth;
    stats.bestScore = std::clamp(bestScoreThisDepth, -MATE + 1, MATE - 1);
    {
      model::Position tmp = pos;
      if (tmp.doMove(bestMoveThisDepth)) {
        stats.bestPV.push_back(bestMoveThisDepth);
        auto rest = build_pv_from_tt(tmp, 32);
        for (auto& mv : rest) stats.bestPV.push_back(mv);
      }
    }
    stats.topMoves.clear();
    for (int i = 0; i < std::min(5, (int)depthCand.size()); ++i)
      stats.topMoves.push_back({depthCand[i].second, depthCand[i].first});

    stableBestScore = stats.bestScore;
    stableBestMove = bestMoveThisDepth;
    if (is_mate_score(stableBestScore)) break;
  }

  stats.bestScore = std::clamp(stableBestScore, -MATE + 1, MATE - 1);
  stats.bestMove = stableBestMove;
  this->stopFlag.reset();
  return stats.bestScore;
}

const SearchStats& Search::getStats() const {
  return stats;
}

void Search::clearSearchState() {
  for (auto& kk : killers) {
    kk[0] = model::Move{};
    kk[1] = model::Move{};
  }
  for (auto& h : history) h.fill(0);
  stats = SearchStats{};
}

}  // namespace lilia::engine
