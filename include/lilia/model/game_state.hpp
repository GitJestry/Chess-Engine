#pragma once
#include <vector>

#include "model_types.hpp"
#include "move.hpp"

namespace lilia {

struct GameState {
  core::Color sideToMove = core::Color::White;
  uint8_t castlingRights = 0b1111;    // WK, WQ, BK, BQ
  core::Square enPassantSquare = 64;  // 0-63 oder 64 = invalid
  int halfmoveClock = 0;
  int fullmoveNumber = 1;

  std::vector<Move> moveHistory;
};

}  // namespace lilia
