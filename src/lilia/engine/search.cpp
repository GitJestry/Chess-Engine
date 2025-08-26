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

#include "lilia/engine/move_order.hpp"

namespace lilia::engine {

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

}  // namespace

struct SearchStoppedException : public std::exception {
  const char* what() const noexcept override { return "Search stopped"; }
};

inline void check_stop(const std::shared_ptr<std::atomic<bool>>& stopFlag) {
  if (stopFlag && stopFlag->load()) throw SearchStoppedException();
}

using steady_clock = std::chrono::steady_clock;

Search::Search(model::TT4& tt_, Evaluator& eval_, const EngineConfig& cfg_)
    : tt(tt_), mg(), cfg(cfg_), evalPtr(&eval_) {
  killers.fill(model::Move{});
  for (auto& h : history) h.fill(0);
  stopFlag.reset();
  stats = SearchStats{};
}

Search::Search(model::TT4& tt_, EvalFactory evalFactory_, const EngineConfig& cfg_)
    : tt(tt_), mg(), cfg(cfg_), evalFactory(std::move(evalFactory_)) {
  if (evalFactory) evalInstance = evalFactory();
  killers.fill(model::Move{});
  for (auto& h : history) h.fill(0);
  stopFlag.reset();
  stats = SearchStats{};
}

Evaluator& Search::currentEval() {
  if (evalPtr) return *evalPtr;

  assert(evalInstance &&
         "Evaluator not initialized; ensure factory provided or legacy Eval passed.");
  return *evalInstance;
}

int Search::signed_eval(model::Position& pos) {
  Evaluator& e = currentEval();
  int v = e.evaluate(pos);
  // we expect evaluate to return White-perspective (positive = White better).
  // For negamax we want positive => side-to-move advantage, so flip if Black to move.
  if (pos.getState().sideToMove == core::Color::Black) return -v;
  return v;
}

static inline bool same_move(const model::Move& a, const model::Move& b) {
  return a.from == b.from && a.to == b.to && a.promotion == b.promotion;
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

int Search::quiescence(model::Position& pos, int alpha, int beta, int ply) {
  stats.nodes++;
  if (stopFlag && stopFlag->load()) {
    int safeEval = signed_eval(pos);
    return std::clamp(safeEval, -MATE, MATE);
  }

  int stand = std::clamp(signed_eval(pos), -MATE, MATE);

  if (stand >= beta) return beta;
  if (alpha < stand) alpha = stand;

  std::vector<model::Move> moves_buf;
  std::vector<model::Move> caps;
  moves_buf.clear();
  caps.clear();

  if (!safeGenerateMoves(mg, pos, moves_buf)) {
    return stand;
  }

  caps.reserve(moves_buf.size());
  for (auto& m : moves_buf) {
    if (m.isCapture || m.promotion != core::PieceType::None) caps.push_back(m);
  }

  std::sort(caps.begin(), caps.end(), [&pos](const model::Move& a, const model::Move& b) {
    return mvv_lva_score(pos, a) > mvv_lva_score(pos, b);
  });

  int best = stand;
  for (auto& m : caps) {
    if (stopFlag && stopFlag->load()) break;
    if (!pos.see(m)) continue;

    MoveUndoGuard g(pos);
    if (!g.doMove(m)) continue;

    int score = -quiescence(pos, -beta, -alpha, ply + 1);
    score = std::clamp(score, -MATE, MATE);

    g.rollback();

    if (score >= beta) return beta;
    if (score > alpha) alpha = score;
    if (score > best) best = score;
  }

  return best;
}

int Search::negamax(model::Position& pos, int depth, int alpha, int beta, int ply,
                    model::Move& refBest) {
  stats.nodes++;
  check_stop(stopFlag);

  if (pos.checkInsufficientMaterial() || pos.checkMoveRule() || pos.checkRepetition()) return 0;

  if (depth <= 0) return quiescence(pos, alpha, beta, ply);

  const int origAlpha = alpha;
  const int origBeta = beta;
  int best = -MATE - 1;
  model::Move bestLocal{};

  // --- TT Probe ---
  model::Move ttMove{};
  bool haveTT = false;
  try {
    if (auto e = tt.probe(pos.hash())) {
      haveTT = true;
      ttMove = e->best;
      if (e->depth >= depth) {
        if (e->bound == model::Bound::Exact) return std::clamp(e->value, -MATE, MATE);
        if (e->bound == model::Bound::Lower) alpha = std::max(alpha, e->value);
        if (e->bound == model::Bound::Upper) beta = std::min(beta, e->value);
        if (alpha >= beta) return std::clamp(e->value, -MATE, MATE);
      }
    }
  } catch (...) {
    haveTT = false;
  }

  // --- Null move pruning ---
  if (depth >= 3 && !pos.inCheck()) {
    NullUndoGuard ng(pos);
    ng.doNull();
    int R = 2;  // standard null-move reduction
    int nullScore = -negamax(pos, depth - 1 - R, -beta, -beta + 1, ply + 1, refBest);
    ng.rollback();
    if (nullScore >= beta) return beta;
  }

  // --- Move generation (legal only) ---
  std::vector<model::Move> moves;
  moves.clear();
  if (!safeGenerateMoves(mg, pos, moves)) {
    if (pos.inCheck()) return -MATE + ply;
    return 0;
  }

  std::vector<model::Move> legal;
  legal.reserve(moves.size());
  for (auto& m : moves) {
    MoveUndoGuard g(pos);
    if (!g.doMove(m)) continue;

    g.rollback();
    legal.push_back(m);
  }

  if (legal.empty()) {
    if (pos.inCheck()) return -MATE + ply;
    return 0;
  }

  // --- Move ordering ---
  std::vector<std::pair<int, model::Move>> scored;
  scored.reserve(legal.size());
  for (auto& m : legal) {
    int score = 0;
    if (haveTT && same_move(ttMove, m))
      score = 20000;
    else if (m.isCapture)
      score = 10000 + mvv_lva_score(pos, m);
    else if (m.promotion != core::PieceType::None)
      score = 9000;
    else if (same_move(m, killers[0]) || same_move(m, killers[1]))
      score = 8000;
    else
      score = history[m.from][m.to];
    scored.push_back({score, m});
  }
  std::sort(scored.begin(), scored.end(), [](auto& a, auto& b) { return a.first > b.first; });

  // --- Search loop ---
  int moveCount = 0;
  for (auto& sm : scored) {
    if (stopFlag && stopFlag->load()) break;

    model::Move m = sm.second;

    MoveUndoGuard g(pos);
    if (!g.doMove(m)) continue;

    int value;
    model::Move childBest{};

    int newDepth = depth - 1;
    if (pos.inCheck()) newDepth += 1;  // check extension

    if (moveCount == 0) {
      value = -negamax(pos, newDepth, -beta, -alpha, ply + 1, childBest);
    } else {
      int reduction = 0;
      if (depth >= 3 && moveCount >= 4 && !m.isCapture && m.promotion == core::PieceType::None)
        reduction = 1;  // LMR

      value = -negamax(pos, newDepth - reduction, -alpha - 1, -alpha, ply + 1, childBest);
      if (value > alpha && value < beta)
        value = -negamax(pos, newDepth, -beta, -alpha, ply + 1, childBest);
    }

    value = std::clamp(value, -MATE, MATE);

    g.rollback();

    if (value > best) {
      best = value;
      bestLocal = m;
    }
    if (value > alpha) alpha = value;

    if (alpha >= beta) {
      if (!m.isCapture && m.promotion == core::PieceType::None) {
        killers[1] = killers[0];
        killers[0] = m;
        history[m.from][m.to] += depth * depth;
      }
      break;
    }
    ++moveCount;
  }

  // --- TT Store ---
  if (!(stopFlag && stopFlag->load())) {
    model::Bound bound;
    if (best <= origAlpha)
      bound = model::Bound::Upper;  // fail-low
    else if (best >= origBeta)
      bound = model::Bound::Lower;  // fail-high
    else
      bound = model::Bound::Exact;  // exact
    try {
      tt.store(pos.hash(), best, static_cast<int16_t>(depth), bound, bestLocal);
    } catch (...) {
    }
  }

  refBest = bestLocal;
  return best;
}

std::vector<model::Move> Search::build_pv_from_tt(model::Position pos, int max_len) {
  std::vector<model::Move> pv;
  for (int i = 0; i < max_len; ++i) {
    auto entry = tt.probe(pos.hash());
    if (!entry) break;

    model::Move m = entry->best;
    if (m.from < 0 || m.to < 0) break;

    if (!pos.doMove(m)) break;

    pv.push_back(m);
  }
  return pv;
}

int Search::search_root_parallel(model::Position& pos, int depth,
                                 std::shared_ptr<std::atomic<bool>> stop, int maxThreads) {
  this->stopFlag = stop;
  stats = SearchStats{};

  // >>> Neue TT-Generation pro Root-Suche (ohne ID)
  // Falls du iterative deepening hast, rufe new_generation() stattdessen pro Iteration auf.
  try {
    tt.new_generation();
  } catch (...) {
  }

  auto start = steady_clock::now();

  std::vector<model::Move> moves;
  moves.clear();
  if (!safeGenerateMoves(mg, pos, moves)) {
    // no workers started, safe to clear stopFlag
    this->stopFlag.reset();
    return 0;
  }

  std::vector<model::Move> legal;
  legal.reserve(moves.size());
  for (auto& m : moves) {
    MoveUndoGuard g(pos);
    if (!g.doMove(m)) continue;
    g.rollback();
    legal.push_back(m);
  }
  if (legal.empty()) {
    this->stopFlag.reset();
    return 0;
  }

  unsigned hw = std::thread::hardware_concurrency();
  if (maxThreads <= 0) maxThreads = (hw > 0 ? (int)hw : 1);
  if ((size_t)maxThreads > legal.size()) maxThreads = static_cast<int>(legal.size());

  struct RootResult {
    int score;
    model::Move move;
    SearchStats stats;
  };

  // futures for running tasks and collected results
  std::vector<std::future<RootResult>> running;
  running.reserve(maxThreads);
  std::vector<RootResult> completedResults;
  completedResults.reserve(legal.size());

  // helper to spawn a worker thread using promise/future so we can join reliably
  auto spawn_worker = [&](model::Move m, model::Position child) -> std::future<RootResult> {
    std::promise<RootResult> prom;
    auto fut = prom.get_future();

    std::thread th([this, child = std::move(child), m = std::move(m), depth, stopPtr = stop,
                    p = std::move(prom)]() mutable {
      try {
        RootResult rr{};
        // Eigene Search-Instanz je Thread (keine gemeinsamen Killer/History/MoveGen)
        Search worker(this->tt, this->evalFactory, this->cfg);
        worker.stopFlag = stopPtr;  // shared_ptr kopieren – hält Flag am Leben

        // Wichtig: KEIN new_generation() pro Worker aufrufen – eine Generation pro Root/Iteration!

        model::Move ref;
        int score = -worker.negamax(child, depth - 1, -INF, INF, 1, ref);
        rr.score = score;
        rr.move = std::move(m);
        rr.stats = worker.getStats();
        p.set_value(std::move(rr));
      } catch (const SearchStoppedException& e) {
        std::cout << "spawn worker search stopped" << std::endl;
        try {
          p.set_exception(std::make_exception_ptr(e));
        } catch (...) {
        }
      } catch (const std::exception& e) {
        try {
          p.set_exception(std::make_exception_ptr(e));
        } catch (...) {
        }
      } catch (...) {
        try {
          p.set_exception(std::make_exception_ptr(std::runtime_error("unknown worker exception")));
        } catch (...) {
        }
      }
    });

    th.detach();
    return fut;
  };

  for (size_t i = 0; i < legal.size(); ++i) {
    if (stop && stop->load()) break;

    model::Move m = std::move(legal[i]);
    model::Position child = pos;
    if (!child.doMove(m)) continue;

    // spawn worker with moved values
    running.push_back(spawn_worker(std::move(m), std::move(child)));

    while ((int)running.size() >= maxThreads) {
      bool foundReady = false;
      for (size_t j = 0; j < running.size(); ++j) {
        auto& fut = running[j];
        if (fut.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready) {
          try {
            RootResult rr = fut.get();
            completedResults.push_back(std::move(rr));
          } catch (const SearchStoppedException&) {
            std::cout << "safe generate search stopped outer" << std::endl;
          } catch (const std::exception& e) {
            std::cerr << "[Search] worker exception: " << e.what() << '\n';
          } catch (...) {
            std::cerr << "[Search] worker unknown exception\n";
          }

          if (j + 1 != running.size()) std::swap(running[j], running.back());
          running.pop_back();
          foundReady = true;
          break;
        }
      }
      if (!foundReady) std::this_thread::yield();
      if (stop && stop->load()) break;
    }
    if (stop && stop->load()) break;
  }

  // collect remaining running futures
  for (auto& fut : running) {
    try {
      RootResult rr = fut.get();
      completedResults.push_back(std::move(rr));
    } catch (const SearchStoppedException&) {
      std::cout << "root result bug" << std::endl;
    } catch (const std::exception& e) {
      std::cerr << "[Search] worker exception during final collect: " << e.what() << '\n';
    } catch (...) {
      std::cerr << "[Search] worker unknown exception during final collect\n";
    }
  }
  running.clear();

  int bestScore = -MATE - 1;
  model::Move bestMove{};
  std::vector<std::pair<int, model::Move>> rootCandidates;
  rootCandidates.reserve(completedResults.size());
  for (auto& rr : completedResults) {
    stats.nodes += rr.stats.nodes;
    rootCandidates.emplace_back(rr.score, std::move(rr.move));
    if (rr.score > bestScore) {
      bestScore = rr.score;
      bestMove = rootCandidates.back().second;
    }
  }

  auto now = steady_clock::now();
  long long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
  stats.elapsedMs = elapsedMs;
  stats.nps = (elapsedMs > 0) ? (double)stats.nodes / (elapsedMs / 1000.0) : (double)stats.nodes;
  stats.bestScore = bestScore;
  stats.bestMove = bestMove;

  std::sort(rootCandidates.begin(), rootCandidates.end(),
            [](auto& a, auto& b) { return a.first > b.first; });
  const size_t K = 5;
  stats.topMoves.clear();
  for (size_t i = 0; i < rootCandidates.size() && i < K; ++i) {
    stats.topMoves.push_back({rootCandidates[i].second, rootCandidates[i].first});
  }

  stats.bestPV.clear();
  if (stats.bestMove.has_value()) {
    model::Position tmp = pos;
    if (tmp.doMove(stats.bestMove.value())) {
      stats.bestPV.push_back(stats.bestMove.value());
      auto rest = build_pv_from_tt(tmp, 32);
      for (auto& mv : rest) stats.bestPV.push_back(mv);
    }
  }

  // all worker threads have been joined via futures above, safe to clear member stopFlag
  this->stopFlag.reset();

  return stats.bestScore;
}

// snapshot stats
const SearchStats& Search::getStats() const {
  return stats;
}

void Search::clearSearchState() {
  killers.fill(model::Move{});
  for (auto& h : history) h.fill(0);
  stats = SearchStats{};
}

}  // namespace lilia::engine
