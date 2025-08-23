#pragma once
#include <array>
#include <atomic>
#include <vector>

#include "../model/position.hpp"
#include "../model/tt4.hpp"
#include "config.hpp"
#include "eval.hpp"
#include "move_order.hpp"

namespace lilia {

struct SearchStats {
  int nodes = 0;
};

class Search {
 public:
  Search(model::TT4& tt, Evaluator& eval, const EngineConfig& cfg);
  int search_root(model::Position& pos, int depth, std::optional<model::Move>& best,
                  std::atomic<bool>* stop = nullptr);

 private:
  int negamax(model::Position& pos, int depth, int alpha, int beta, int ply, model::Move& refBest);
  int quiescence(model::Position& pos, int alpha, int beta, int ply);

  model::TT4& tt;
  Evaluator& eval;
  const EngineConfig cfg;
  SearchStats stats;

  // killer and history heuristic
  std::array<model::Move, 2> killers;           // per ply (simple)
  std::array<std::array<int, 64>, 64> history;  // history[piecetype][to]
  std::atomic<bool>* stopFlag = nullptr;        // nullptr == kein Stopp gewünscht
};

}  // namespace lilia
