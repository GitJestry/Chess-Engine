#pragma once

#include <array>

#include "model_types.hpp"

namespace lilia {

namespace chess {

class Board {
 public:
  Board();

  void setPiece(core::Square sq, core::Piece piece);
  void removePiece(core::Square sq);
  core::Piece getPiece(core::Square sq) const;

  core::Bitboard occupancy(core::Color color) const;
  core::Bitboard allPieces() const;

 private:
  std::array<core::Bitboard, 6> pieceBitboards[2];  // [color][PieceType]
};

}  // namespace chess

}  // namespace lilia
