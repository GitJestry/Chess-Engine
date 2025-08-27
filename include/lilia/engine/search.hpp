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

static const int INF = 30000;
static const int MATE = 29000;

struct SearchStats {
  int nodes = 0;
  double nps = 0.0;
  long long elapsedMs = 0;
  int bestScore = 0;
  std::optional<model::Move> bestMove;
  std::vector<std::pair<model::Move, int>> topMoves;
  std::vector<model::Move> bestPV;
};

class Evaluator;

class Search {
 public:
  Search(model::TT4& tt, std::shared_ptr<const Evaluator> eval, const EngineConfig& cfg);
  ~Search() = default;

  int search_root_parallel(model::Position& pos, int depth, std::shared_ptr<std::atomic<bool>> stop,
                           int maxThreads = 0);

  const SearchStats& getStats() const;
  void clearSearchState();

  model::TT4& ttRef() { return tt; }

 private:
  int negamax(model::Position& pos, int depth, int alpha, int beta, int ply, model::Move& refBest);
  int quiescence(model::Position& pos, int alpha, int beta, int ply);
  std::vector<model::Move> build_pv_from_tt(model::Position pos, int max_len = 16);
  int signed_eval(model::Position& pos);

  model::TT4& tt;
  model::MoveGenerator mg;
  const EngineConfig& cfg;

  std::shared_ptr<const Evaluator> eval_;

  std::array<model::Move, 2> killers;
  std::array<std::array<int, 64>, 64> history;

  std::shared_ptr<std::atomic<bool>> stopFlag;
  SearchStats stats;
};

}  // namespace lilia::engine
