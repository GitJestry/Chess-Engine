#pragma once

#include "player.hpp"

namespace lilia::controller {

class BotPlayer : public IPlayer {
 public:
  explicit BotPlayer(int thinkMillis = 300, int depth = 8)
      : m_thinkMillis(thinkMillis), m_depth(depth) {}
  ~BotPlayer() override = default;

  bool isHuman() const override { return false; }
  std::future<model::Move> requestMove(model::ChessGame& gameState,
                                       std::atomic<bool>& cancelToken) override;

 private:
  int m_depth;
  int m_thinkMillis;
};

}  // namespace lilia::controller
