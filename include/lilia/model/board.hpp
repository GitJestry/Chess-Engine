#pragma once
#include <array>
#include <optional>

#include "core/model_types.hpp"

namespace lilia::model {

/// 12 piece bitboards: [color][PieceType] for Pawn..King (None unused)
class Board {
 public:
  Board();

  void clear();

  void setPiece(core::Square sq, bb::Piece p);
  void removePiece(lilia::core::Square sq);
  std::optional<bb::Piece> getPiece(core::Square sq) const;

  bb::Bitboard pieces(core::Color c) const { return m_color_occ[bb::ci(c)]; }
  bb::Bitboard allPieces() const { return m_all_occ; }
  bb::Bitboard pieces(core::Color c, core::PieceType t) const {
    return m_bb[bb::ci(c)][static_cast<int>(t)];
  }

 private:
  // [color][type] where type indices 0..5 are Pawn..King
  std::array<std::array<bb::Bitboard, 6>, 2> m_bb{};
  std::array<bb::Bitboard, 2> m_color_occ{};
  bb::Bitboard m_all_occ = 0;

  void recomputeOccupancy();
};

}  // namespace lilia::model
