#pragma once
#include <array>
#include <optional>

#include "core/model_types.hpp"

namespace lilia::model {

class Board {
 public:
  Board();

  void clear();

  void setPiece(core::Square sq, bb::Piece p);
  void removePiece(lilia::core::Square sq);
  std::optional<bb::Piece> getPiece(core::Square sq) const;

  bb::Bitboard getPieces(core::Color c) const { return m_color_occ[bb::ci(c)]; }
  bb::Bitboard getAllPieces() const { return m_all_occ; }
  bb::Bitboard getPieces(core::Color c, core::PieceType t) const {
    return m_bb[bb::ci(c)][static_cast<int>(t)];
  }

 private:
  std::array<std::array<bb::Bitboard, 6>, 2> m_bb{};
  std::array<bb::Bitboard, 2> m_color_occ{};
  bb::Bitboard m_all_occ = 0;

  void recomputeOccupancy();
};

}  // namespace lilia::model
