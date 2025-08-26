#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "../model/move_generator.hpp"
#include "../model/position.hpp"
#include "../model/tt4.hpp"
#include "config.hpp"
#include "eval.hpp"

namespace lilia::engine {

// small constants
static const int INF = 30000;
static const int MATE = 29000;

struct SearchStats {
  int nodes = 0;                                      // total nodes searched (last run)
  double nps = 0.0;                                   // nodes per second (last run)
  long long elapsedMs = 0;                            // elapsed milliseconds (last run)
  int bestScore = 0;                                  // score (centi) for bestMove (negamax conv.)
  std::optional<model::Move> bestMove;                // best move found (last run)
  std::vector<std::pair<model::Move, int>> topMoves;  // top-K moves (move, score)
  std::vector<model::Move> bestPV;                    // principal variation for bestMove
};

class Evaluator;  // forward (included via "eval.hpp", but keep forward for safety)

// Search class supports either a single shared Evaluator& (legacy) OR a factory that
// produces per-thread Evaluator instances (recommended for MT).
class Search {
 public:
  using EvalFactory = std::function<std::unique_ptr<Evaluator>()>;

  // Legacy ctor: use an existing Evaluator reference (shared). Evaluator must be thread-safe
  // if using parallel search with this constructor.
  Search(model::TT4& tt, Evaluator& eval, const EngineConfig& cfg);

  // New ctor: provide a factory that produces per-thread Evaluator instances.
  // Search will call the factory for the main thread and worker threads will create
  // their own Evaluators via this factory.
  Search(model::TT4& tt, EvalFactory evalFactory, const EngineConfig& cfg);

  ~Search() = default;

  // parallel root search (uses EvalFactory if provided; otherwise uses shared Evaluator&).
  int search_root_parallel(model::Position& pos, int depth, std::atomic<bool>* stop,
                           int maxThreads = 0);

  // get snapshot of stats
  SearchStats getStatsCopy() const;

  // clear/initialize search internal state (killers/history/stats)
  void clearSearchState();

  // get reference to underlying tt (if needed)
  model::TT4& ttRef() { return tt; }

 private:
  // core search routines (kept private)
  int negamax(model::Position& pos, int depth, int alpha, int beta, int ply, model::Move& refBest);
  int quiescence(model::Position& pos, int alpha, int beta, int ply);

  // helper: rebuild PV from TT (starting after given move already applied)
  std::vector<model::Move> build_pv_from_tt(model::Position pos, int max_len = 16);

  // helper accessor: returns the evaluator instance to use for this Search object.
  // - If constructed with legacy Evaluator&, returns that reference.
  // - If constructed with EvalFactory, returns the per-Search evalInstance (main thread).
  Evaluator& currentEval();

  // convert Evaluator::evaluate (white-perspective) to negamax sign convention
  int signed_eval(model::Position& pos);

  // members
  model::TT4& tt;
  model::MoveGenerator mg;
  const EngineConfig& cfg;

  // Evaluator handling:
  // - evalPtr: non-owning pointer to a shared Evaluator (legacy ctor).
  // - evalFactory: if provided, used to create per-thread Evaluators.
  // - evalInstance: main-thread owned Evaluator when factory is used.
  Evaluator* evalPtr = nullptr;
  EvalFactory evalFactory;
  std::unique_ptr<Evaluator> evalInstance = nullptr;

  // search state
  std::array<model::Move, 2> killers;
  std::array<std::array<int, 64>, 64> history;

  std::atomic<bool>* stopFlag = nullptr;
  SearchStats stats;
};

}  // namespace lilia::engine
