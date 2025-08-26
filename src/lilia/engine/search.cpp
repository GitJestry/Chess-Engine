#include "lilia/engine/search.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <exception>
#include <future>
#include <iostream>
#include <limits>
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

// === cancellation exception ===
struct SearchStoppedException : public std::exception {
  const char* what() const noexcept override { return "Search stopped"; }
};

inline void check_stop(std::atomic<bool>* stopFlag) {
  if (stopFlag && stopFlag->load()) throw SearchStoppedException();
}

using steady_clock = std::chrono::steady_clock;

// ------------------ Constructors ------------------
Search::Search(model::TT4& tt_, Evaluator& eval_, const EngineConfig& cfg_)
    : tt(tt_), mg(), cfg(cfg_), evalPtr(&eval_) {
  killers.fill(model::Move{});
  for (auto& h : history) h.fill(0);
  stopFlag = nullptr;
  stats = SearchStats{};
}

Search::Search(model::TT4& tt_, EvalFactory evalFactory_, const EngineConfig& cfg_)
    : tt(tt_), mg(), cfg(cfg_), evalFactory(std::move(evalFactory_)) {
  // create a main-thread evaluator instance from factory
  if (evalFactory) evalInstance = evalFactory();
  killers.fill(model::Move{});
  for (auto& h : history) h.fill(0);
  stopFlag = nullptr;
  stats = SearchStats{};
}

// currentEval: returns the evaluator used by this Search instance (non-owning reference)
Evaluator& Search::currentEval() {
  if (evalPtr) return *evalPtr;
  // else we must have an evalInstance (created from factory) for main thread
  assert(evalInstance &&
         "Evaluator not initialized; ensure factory provided or legacy Eval passed.");
  return *evalInstance;
}

// signed_eval: convert evaluator result (White perspective) to negamax sign convention
int Search::signed_eval(model::Position& pos) {
  Evaluator& e = currentEval();
  int v = e.evaluate(pos);
  // we expect evaluate to return White-perspective (positive = White better).
  // For negamax we want positive => side-to-move advantage, so flip if Black to move.
  if (pos.state().sideToMove == core::Color::Black) return -v;
  return v;
}

// ------------------ helpers ------------------
static inline bool same_move(const model::Move& a, const model::Move& b) {
  return a.from == b.from && a.to == b.to && a.promotion == b.promotion;
}

static inline bool safeGenerateMoves(model::MoveGenerator& mg, model::Position& pos,
                                     std::vector<model::Move>& out) {
  // ensure minimal capacity so generator doesn't reallocate (cheap check)
  if (out.capacity() < 128) out.reserve(128);
  out.clear();
  try {
    mg.generatePseudoLegalMoves(pos.board(), pos.state(), out);
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

  // local buffers (safe for multiple concurrent Searches)
  std::vector<model::Move> moves_buf;
  std::vector<model::Move> caps;
  moves_buf.clear();
  caps.clear();

  if (!safeGenerateMoves(mg, pos, moves_buf)) {
    return stand;
  }

  // collect captures & promotions
  caps.reserve(moves_buf.size());
  for (auto& m : moves_buf) {
    if (m.isCapture || m.promotion != core::PieceType::None) caps.push_back(m);
  }

  // MVV-LVA sort (descending)
  std::sort(caps.begin(), caps.end(), [&pos](const model::Move& a, const model::Move& b) {
    return mvv_lva_score(pos, a) > mvv_lva_score(pos, b);
  });

  int best = stand;
  for (auto& m : caps) {
    if (stopFlag && stopFlag->load()) break;
    if (!pos.see(m)) continue;

    MoveUndoGuard g(pos);
    if (!g.doMove(m)) continue;

    int score = -quiescence(pos, -beta, -alpha, ply + 1);  // g sorgt für Undo auch bei Throw
    score = std::clamp(score, -MATE, MATE);

    // undo (guard destructor will also undo if needed)
    g.rollback();

    if (score >= beta) return beta;
    if (score > alpha) alpha = score;
    if (score > best) best = score;
  }

  return best;
}

// ------------------ negamax ------------------
int Search::negamax(model::Position& pos, int depth, int alpha, int beta, int ply,
                    model::Move& refBest) {
  stats.nodes++;
  check_stop(stopFlag);  // statt return 0

  if (pos.checkInsufficientMaterial() || pos.checkMoveRule() || pos.checkRepetition()) return 0;

  if (depth <= 0) return quiescence(pos, alpha, beta, ply);

  const int origAlpha = alpha;
  const int origBeta = beta;
  int best = -MATE - 1;
  model::Move bestLocal{};

  // TT probe (protected by mutex)
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

  // null move (exception-safe guard)
  if (depth >= 3 && !pos.inCheck()) {
    NullUndoGuard ng(pos);
    ng.doNull();
    int nullScore = -negamax(pos, depth - 1 - 2, -beta, -beta + 1, ply + 1, refBest);
    ng.rollback();
    if (nullScore >= beta) return beta;  // ng wird automatisch rückgängig gemacht
  }

  // generate moves in safe manner
  std::vector<model::Move> moves;
  moves.clear();
  if (!safeGenerateMoves(mg, pos, moves)) {
    // treat as no moves if generation failed
    if (pos.inCheck()) return -MATE + ply;
    return 0;
  }

  std::vector<model::Move> legal;
  legal.reserve(moves.size());
  for (auto& m : moves) {
    // use MoveUndoGuard locally to ensure we always undo if doMove succeeded but later throws
    MoveUndoGuard g(pos);
    if (!g.doMove(m)) continue;
    // explicit rollback to leave pos unchanged for next iterate
    g.rollback();
    legal.push_back(m);
  }

  if (legal.empty()) {
    if (pos.inCheck()) return -MATE + ply;
    return 0;
  }

  // move ordering
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
        reduction = 1;

      value = -negamax(pos, newDepth - reduction, -alpha - 1, -alpha, ply + 1, childBest);
      if (value > alpha && value < beta)
        value = -negamax(pos, newDepth, -beta, -alpha, ply + 1, childBest);
    }

    value = std::clamp(value, -MATE, MATE);

    // explicit rollback of move before continuing iteration
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

  // store TT (protected by mutex)
  if (!(stopFlag && stopFlag->load())) {
    model::Bound bound;
    if (best <= origAlpha)
      bound = model::Bound::Upper;
    else if (best >= origBeta)
      bound = model::Bound::Lower;
    else
      bound = model::Bound::Exact;
    try {
      tt.store(pos.hash(), best, static_cast<int16_t>(depth), bound, bestLocal);
    } catch (...) {
      // ignore storage failures
    }
  }

  refBest = bestLocal;
  return best;
}

std::vector<model::Move> Search::build_pv_from_tt(model::Position pos, int max_len) {
  std::vector<model::Move> pv;
  for (int i = 0; i < max_len; ++i) {
    auto entry = tt.probe(pos.hash());
    if (!entry) break;  // Kein TT-Eintrag vorhanden

    model::Move m = entry->best;
    if (m.from < 0 || m.to < 0) break;  // Ungültiger Move

    if (!pos.doMove(m)) break;  // Move kann nicht ausgeführt werden

    pv.push_back(m);
  }
  return pv;
}

int Search::search_root_parallel(model::Position& pos, int depth, std::atomic<bool>* stop,
                                 int maxThreads) {
  // set stop pointer for main search instance (workers will capture 'stop' explicitly)
  this->stopFlag = stop;
  stats = SearchStats{};

  auto start = steady_clock::now();

  // local move generation buffer
  std::vector<model::Move> moves;
  moves.clear();
  if (!safeGenerateMoves(mg, pos, moves)) {
    // no workers started, safe to clear stopFlag
    this->stopFlag = nullptr;
    return 0;
  }

  std::vector<model::Move> legal;
  legal.reserve(moves.size());
  for (auto& m : moves) {
    MoveUndoGuard g(pos);
    if (!g.doMove(m)) continue;
    g.rollback();
    legal.push_back(m);  // we will std::move from legal later
  }
  if (legal.empty()) {
    this->stopFlag = nullptr;
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

  // running threads and collected results
  // running: pairs (thread, future)
  std::vector<std::pair<std::thread, std::future<RootResult>>> running;
  running.reserve(maxThreads);
  std::vector<RootResult> completedResults;
  completedResults.reserve(legal.size());

  // helper to spawn a worker thread using promise/future so we can join reliably
  auto spawn_worker = [&](model::Move m, model::Position child, std::atomic<bool>* stopPtr) {
    std::promise<RootResult> prom;
    auto fut = prom.get_future();

    std::thread th([this, child = std::move(child), m = std::move(m), depth, stopPtr,
                    p = std::move(prom)]() mutable {
      // note: promise was moved into lambda capture (named 'p')
      try {
        RootResult rr{};
        Search worker(this->tt, this->evalFactory, this->cfg);
        worker.stopFlag = stopPtr;  // safe copy of pointer
        model::Move ref;
        int score = -worker.negamax(child, depth - 1, -INF, INF, 1, ref);
        rr.score = score;
        rr.move = std::move(m);
        rr.stats = worker.getStatsCopy();
        p.set_value(std::move(rr));
      } catch (const SearchStoppedException& e) {
        std::cout << "spawn worker search stopped" << std::endl;
        try {
          p.set_exception(std::make_exception_ptr(e));
        } catch (...) {
          // ignore set_exception failure
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

    running.emplace_back(std::move(th), std::move(fut));
  };

  // Launch workers in waves, moving `legal` entries into the workers
  for (size_t i = 0; i < legal.size(); ++i) {
    if (stop && stop->load()) break;

    // move the root move out of the vector to avoid copy
    model::Move m = std::move(legal[i]);
    model::Position child = pos;     // copy here; we'll move into thread
    if (!child.doMove(m)) continue;  // if illegal after all, skip

    // spawn worker with moved values (pass stop pointer explicitly)
    spawn_worker(std::move(m), std::move(child), stop);

    // ensure we don't exceed concurrency limit
    while ((int)running.size() >= maxThreads) {
      bool foundReady = false;
      for (size_t j = 0; j < running.size(); ++j) {
        auto& pr = running[j];
        auto& fut = pr.second;
        if (fut.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready) {
          // join thread first (ensures resources cleaned up)
          if (pr.first.joinable()) pr.first.join();
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
          // remove this entry by swap/pop_back
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

  // collect remaining running threads
  for (auto& pr : running) {
    auto& th = pr.first;
    auto& fut = pr.second;
    if (th.joinable()) th.join();
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

  // compute best from completed results
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

  // finalize stats
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

  // all worker threads have been joined above, safe to clear member stopFlag
  this->stopFlag = nullptr;
  return stats.bestScore;
}

// snapshot stats
SearchStats Search::getStatsCopy() const {
  return stats;
}

void Search::clearSearchState() {
  killers.fill(model::Move{});
  for (auto& h : history) h.fill(0);
  stats = SearchStats{};
}

}  // namespace lilia::engine
