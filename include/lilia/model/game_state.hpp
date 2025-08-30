#pragma once
#include <vector>

#include "core/model_types.hpp"
#include "move.hpp"

namespace lilia::model {

struct GameState {
  bb::Bitboard pawnKey = 0;          // IMPORTANT: initialize
  std::uint32_t fullmoveNumber = 1;  // 1..2^32-1 is plenty
  std::uint16_t halfmoveClock = 0;   // 0..100 fits in u16
  std::uint8_t castlingRights =
      bb::Castling::WK | bb::Castling::WQ | bb::Castling::BK | bb::Castling::BQ;
  core::Color sideToMove = core::Color::White;
  core::Square enPassantSquare = core::NO_SQUARE;
};

struct StateInfo {
  Move move{};  // unchanged
  bb::Bitboard prevPawnKey{};
  bb::Bitboard zobristKey{};
  bb::Piece captured{};  // unchanged
  std::uint16_t prevHalfmoveClock{};
  std::uint8_t gaveCheck{0};
  std::uint8_t prevCastlingRights{};  // unchanged

  core::Square prevEnPassantSquare{core::NO_SQUARE};
};

struct NullState {
  bb::Bitboard zobristKey;
  int prevHalfmoveClock;
  int prevFullmoveNumber;
  std::uint8_t prevCastlingRights;
  core::Square prevEnPassantSquare;
};

}  // namespace lilia::model
