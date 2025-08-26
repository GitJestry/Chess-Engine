#pragma once

#include <atomic>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../model/move.hpp"
#include "../model/chess_game.hpp"
#include "engine.hpp"
#include "search.hpp"

namespace lilia::engine {

struct SearchResult {
  std::optional<model::Move> bestMove;
  engine::SearchStats stats;
  
  std::vector<std::pair<model::Move, int>> topMoves;
};

class BotEngine {
 public:
  BotEngine();
  ~BotEngine();

  
  
  SearchResult findBestMove(model::ChessGame& gameState, int maxDepth, int thinkMillis,
                            std::atomic<bool>* externalCancel = nullptr);

  
  engine::SearchStats getLastSearchStats() const;

 private:
  Engine m_engine;
};

}  
