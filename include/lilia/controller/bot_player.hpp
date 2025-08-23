#pragma once

#include <chrono>
#include <memory>
#include <thread>

#include "player.hpp"

namespace lilia::controller {

class BotPlayer : public IPlayer {
 public:
  // thinkMillis: heuristische Zeit, die die KI zum "Denken" verwenden soll.
  explicit BotPlayer(int thinkMillis = 300) : m_thinkMillis(thinkMillis) {}
  ~BotPlayer() override = default;

  bool isHuman() const override { return false; }

  std::future<model::Move> requestMove(model::ChessGame& gameState,
                                       std::atomic<bool>& cancelToken) override;

 private:
  int m_thinkMillis;
};

}  // namespace lilia::controller
