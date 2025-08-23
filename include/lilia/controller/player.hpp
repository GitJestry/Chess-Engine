#pragma once

#include <atomic>
#include <future>

#include "../model/chess_game.hpp"

namespace lilia::controller {

/**
 * @brief Interface for a Player (Human or Bot).
 *
 * - Human implementations may return a default/invalid future or use a different
 * flow; GameManager treats a nullptr or isHuman() == true as a human.
 * - Bot implementations should respect the cancelToken and return a valid
 * model::Move (or an invalid/empty move) when cancelled/finished.
 */
struct IPlayer {
  virtual ~IPlayer() = default;

  // Liefert asynchron den nächsten Move. Die Implementierung muss regelmäßig
  // cancelToken prüfen und bei Abbruch einen invaliden Move zurückgeben.
  virtual std::future<model::Move> requestMove(model::ChessGame& gameState,
                                               std::atomic<bool>& cancelToken) = 0;

  // True wenn dieser Player ein Mensch ist (GameManager startet dann keinen Bot).
  virtual bool isHuman() const = 0;
};

}  // namespace lilia::controller
