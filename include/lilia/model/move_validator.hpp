#pragma once
#include "board.hpp"
#include "game_state.hpp"
#include "move.hpp"

namespace lilia {

class MoveValidator {
 public:
  bool isLegal(const Board& board, const GameState& state, const Move& move) const;
};

}  // namespace lilia
