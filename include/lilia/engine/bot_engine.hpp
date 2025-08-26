#pragma once

#include <atomic>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../model/Move.hpp"
#include "../model/chess_game.hpp"
#include "engine.hpp"
#include "search.hpp"

namespace lilia::engine {

struct SearchResult {
  std::optional<model::Move> bestMove;
  engine::SearchStats stats;
  // topMoves sind meist schon in stats.topMoves, aber kept here for convenience
  std::vector<std::pair<model::Move, int>> topMoves;
};

class BotEngine {
 public:
  explicit BotEngine(const EngineConfig& cfg = {});
  ~BotEngine();

  // Führe die Suche aus. thinkMillis = max Denkzeit in ms (0 = no timer).
  // externalCancel kann nullptr sein; wenn gesetzt, wird diese atomische bool geprüft.
  SearchResult findBestMove(model::ChessGame& gameState, int maxDepth, int thinkMillis,
                            std::atomic<bool>* externalCancel = nullptr);

  // Direkt zugänglich, falls jemand Stats separat lesen will
  engine::SearchStats getLastSearchStats() const;

 private:
  Engine m_engine;
};

}  // namespace lilia::engine
