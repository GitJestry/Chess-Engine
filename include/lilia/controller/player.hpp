#pragma once

#include <atomic>
#include <future>

#include "../model/chess_game.hpp"

namespace lilia::controller {

struct IPlayer {
  virtual ~IPlayer() = default;

  
  
  virtual std::future<model::Move> requestMove(model::ChessGame& gameState,
                                               std::atomic<bool>& cancelToken) = 0;

  
  virtual bool isHuman() const = 0;
};

}  
