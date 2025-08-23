#include "lilia/controller/bot_player.hpp"

#include <random>

namespace lilia::controller {

std::future<model::Move> BotPlayer::requestMove(model::ChessGame& gameState,
                                                std::atomic<bool>& cancelToken) {
  // Simulierter asynchroner Denk-Job. Eine echte Engine würde hier iterativ
  // tiefer rechnen, regelmäßig cancelToken prüfen und am Ende den besten Move
  // zurückgeben. Wir geben als Fallback den ersten legalen Move.
  return std::async(std::launch::async, [this, &gameState, &cancelToken]() -> model::Move {
    const int stepMs = 10;
    int iterations = m_thinkMillis / stepMs;
    for (int i = 0; i < iterations; ++i) {
      if (cancelToken.load()) return model::Move{};  // Abbruch -> invalider Move
      std::this_thread::sleep_for(std::chrono::milliseconds(stepMs));
    }

    // einfache Heuristik: nimm den ersten legalen Move
    auto moves = gameState.generateLegalMoves();
    if (!moves.empty()) return moves.front();
    return model::Move{};
  });
}

}  // namespace lilia::controller
