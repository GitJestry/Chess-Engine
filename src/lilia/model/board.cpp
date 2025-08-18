#include "lilia/model/board.hpp"

#include <cassert>

namespace lilia::model {
Board::Board() {
  clear();
}

void Board::clear() {
  for (auto& byColor : m_bb) byColor.fill(0);
  m_color_occ = {0, 0};
  m_all_occ = 0;
}

static inline int type_index(core::PieceType t) {
  switch (t) {
    case core::PieceType::Pawn:
      return 0;
    case core::PieceType::Knight:
      return 1;
    case core::PieceType::Bishop:
      return 2;
    case core::PieceType::Rook:
      return 3;
    case core::PieceType::Queen:
      return 4;
    case core::PieceType::King:
      return 5;
    default:
      return -1;
  }
}

void Board::recomputeOccupancy() {
  m_color_occ[0] = m_color_occ[1] = 0;
  for (int c = 0; c < 2; ++c) {
    for (int t = 0; t < 6; ++t) m_color_occ[c] |= m_bb[c][t];
  }
  m_all_occ = m_color_occ[0] | m_color_occ[1];
}

void Board::setPiece(core::Square sq, bb::Piece p) {
  removePiece(sq);  // ensure empty first
  if (p.type == core::PieceType::None) return;

  int ti = type_index(p.type);
  assert(ti >= 0 && "Invalid PieceType");

  m_bb[bb::ci(p.color)][ti] |= bb::sq_bb(sq);
  recomputeOccupancy();
}

void Board::removePiece(core::Square sq) {
  bb::Bitboard mask = ~bb::sq_bb(sq);
  for (int c = 0; c < 2; ++c)
    for (int t = 0; t < 6; ++t) m_bb[c][t] &= mask;
  recomputeOccupancy();
}

std::optional<bb::Piece> Board::getPiece(core::Square sq) const {
  bb::Bitboard bbSq = bb::sq_bb(sq);
  for (int c = 0; c < 2; ++c) {
    if ((m_color_occ[c] & bbSq) == 0) continue;
    for (int t = 0; t < 6; ++t)
      if (m_bb[c][t] & bbSq)
        return bb::Piece{static_cast<core::PieceType>(t),
                         c == 0 ? core::Color::White : core::Color::Black};
  }
  return std::nullopt;
}

}  // namespace lilia::model
