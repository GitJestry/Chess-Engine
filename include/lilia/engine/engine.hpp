#pragma once

#include <atomic>
#include <optional>

#include "../model/move.hpp"
#include "../model/position.hpp"
#include "config.hpp"

namespace lilia {

class Engine {
 public:
  explicit Engine(const EngineConfig& cfg = {});
  ~Engine();

  // Find best move for the position (returns empty if none)

  std::optional<model::Move> find_best_move(model::Position& pos, int maxDepth = 20,
                                            std::atomic<bool>* stop = nullptr);

 private:
  struct Impl;
  Impl* pimpl;
};

}  // namespace lilia
