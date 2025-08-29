
#pragma once
#include "../chess_types.hpp"
#include "board.hpp"
#include "core/bitboard.hpp"
#include "core/magic.hpp"

namespace lilia::model {
// ---------------- Angriffsabfrage ----------------

static inline bool attackedBy(const Board& b, core::Square sq, core::Color by,
                              bb::Bitboard occ) noexcept {
  const bb::Bitboard target = bb::sq_bb(sq);
  occ &= ~target;  // <— Ziel-Feld aus der Belegung maskieren

  // Pawn
  const bb::Bitboard pawns = b.getPieces(by, core::PieceType::Pawn);
  const bb::Bitboard pawnAtkToSq = (by == core::Color::White) ? (bb::sw(target) | bb::se(target))
                                                              : (bb::nw(target) | bb::ne(target));
  if (pawnAtkToSq & pawns) return true;

  if (bb::knight_attacks_from(sq) & b.getPieces(by, core::PieceType::Knight)) return true;

  const bb::Bitboard diag = magic::sliding_attacks(magic::Slider::Bishop, sq, occ);
  if (diag & (b.getPieces(by, core::PieceType::Bishop) | b.getPieces(by, core::PieceType::Queen)))
    return true;

  const bb::Bitboard ortho = magic::sliding_attacks(magic::Slider::Rook, sq, occ);
  if (ortho & (b.getPieces(by, core::PieceType::Rook) | b.getPieces(by, core::PieceType::Queen)))
    return true;

  if (bb::king_attacks_from(sq) & b.getPieces(by, core::PieceType::King)) return true;
  return false;
}

}  // namespace lilia::model
