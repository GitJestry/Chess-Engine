#pragma once
#include <vector>

#include "core/model_types.hpp"
#include "move.hpp"

namespace lilia::model {

struct GameState {
  core::Color sideToMove = core::Color::White;
  std::uint8_t castlingRights =
      bb::Castling::WK | bb::Castling::WQ | bb::Castling::BK | bb::Castling::BQ;
  core::Square enPassantSquare = core::NO_SQUARE;
  std::uint16_t halfmoveClock = 0;   // 0..100 fits in u16
  std::uint32_t fullmoveNumber = 1;  // 1..2^32-1 is plenty
  bb::Bitboard pawnKey = 0;          // IMPORTANT: initialize
};

struct StateInfo {
  Move move{};           // unchanged
  bb::Piece captured{};  // unchanged

  std::uint8_t prevCastlingRights{};  // unchanged
  core::Square prevEnPassantSquare{core::NO_SQUARE};
  std::uint16_t prevHalfmoveClock{};

  bb::Bitboard zobristKey{};
  bb::Bitboard prevPawnKey{};

  std::uint8_t gaveCheck{0};
};

}  // namespace lilia::model
