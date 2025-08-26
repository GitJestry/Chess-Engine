#pragma once

#include <atomic>
#include <optional>

#include "../model/core/magic.hpp"
#include "../model/move.hpp"
#include "../model/position.hpp"
#include "config.hpp"

namespace lilia::engine {
struct SearchStats;

class Engine {
 public:
  explicit Engine(const EngineConfig& cfg = {});
  ~Engine();

  // Find best move for the position (returns empty if none)
  static void init() {
    model::Zobrist::init();
    model::magic::init_magics();
  }
  std::optional<model::Move> find_best_move(model::Position& pos, int maxDepth = 8,
                                            std::atomic<bool>* stop = nullptr);
  const SearchStats& getLastSearchStats() const;
  const EngineConfig& getConfig() const;

 private:
  struct Impl;
  Impl* pimpl;
};

}  // namespace lilia::engine
