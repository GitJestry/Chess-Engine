#pragma once
#include <array>
#include <atomic>
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

class Search {
 public:
  Search(model::TT4& tt, Evaluator& eval, const EngineConfig& cfg);

  // run search from root position up to `depth`. best is returned via bestOut.
  // stop is optional cooperative stop flag (nullptr = no stop).
  int search_root(model::Position& pos, int depth, std::atomic<bool>* stop = nullptr);
  int search_root_parallel(model::Position& pos, int depth, std::atomic<bool>* stop,
                           int maxThreads = 0);  // parallel root search
  // get snapshot of stats
  SearchStats getStatsCopy() const;

  // get reference to underlying tt (if needed)
  model::TT4& ttRef() { return tt; }

 private:
  int negamax(model::Position& pos, int depth, int alpha, int beta, int ply, model::Move& refBest);
  int quiescence(model::Position& pos, int alpha, int beta, int ply);

  // helper: rebuild PV from TT (starting after given move already applied)
  std::vector<model::Move> build_pv_from_tt(model::Position pos, int max_len = 16);

  model::TT4& tt;
  Evaluator& eval;
  const EngineConfig& cfg;

  model::MoveGenerator mg;

  std::array<model::Move, 2> killers;
  std::array<std::array<int, 64>, 64> history;

  std::atomic<bool>* stopFlag;
  SearchStats stats;
};

}  // namespace lilia::engine
