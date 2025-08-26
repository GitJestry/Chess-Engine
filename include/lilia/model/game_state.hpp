#pragma once
#include <vector>

#include "core/model_types.hpp"
#include "move.hpp"

namespace lilia::model {

struct GameState {
  core::Color sideToMove = core::Color::White;
  std::uint8_t castlingRights = bb::Castling::WK | bb::Castling::WQ | bb::Castling::BK |
                                bb::Castling::BQ;  // see chess_types Castling
  core::Square enPassantSquare = core::NO_SQUARE;  // 0..63 or core::NO_SQUARE = invalid
  int halfmoveClock = 0;
  int fullmoveNumber = 1;
  bb::Bitboard pawnKey;
};

struct StateInfo {
  Move move{};
  bb::Piece captured{};  // captured piece (for undo)
  std::uint8_t prevCastlingRights{};
  core::Square prevEnPassantSquare{core::NO_SQUARE};
  int prevHalfmoveClock{};
  bb::Bitboard zobristKey{};
  bb::Bitboard prevPawnKey;
};

}  // namespace lilia::model
