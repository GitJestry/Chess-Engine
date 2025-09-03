#pragma once
#include <array>
#include <optional>

#include "core/model_types.hpp"

namespace lilia::model {

class Board {
 public:
  Board();

  void clear() noexcept;

  // API unverändert
  void setPiece(core::Square sq, bb::Piece p) noexcept;
  void removePiece(core::Square sq) noexcept;
  std::optional<bb::Piece> getPiece(core::Square sq) const noexcept;

  bb::Bitboard getPieces(core::Color c) const { return m_color_occ[bb::ci(c)]; }
  bb::Bitboard getAllPieces() const { return m_all_occ; }
  bb::Bitboard getPieces(core::Color c, core::PieceType t) const {
    return m_bb[bb::ci(c)][static_cast<int>(t)];
  }

  // Optionaler Fast-Path (später in Position nutzen):
  // bewegt eine Figur von 'from' nach 'to' (ohne Capture; 'to' muss leer sein)
  void movePiece_noCapture(core::Square from, core::Square to) noexcept;
  // Move a piece from `from` to `to` while removing a captured piece on `capSq`.
  // For normal captures: capSq == to.
  // For en passant:      capSq != to (the pawn behind `to`).
  // `captured` must describe the piece removed on `capSq` (type != None).
  void movePiece_withCapture(core::Square from, core::Square capSq, core::Square to,
                             bb::Piece captured) noexcept;

 private:
  // Bitboards wie gehabt
  std::array<std::array<bb::Bitboard, 6>, 2> m_bb{};
  std::array<bb::Bitboard, 2> m_color_occ{};
  bb::Bitboard m_all_occ = 0;

  // Neu: O(1)-Lookup pro Feld (0 = leer, sonst (ptIdx+1) | (color<<3))
  std::array<std::uint8_t, 64> m_piece_on{};

  // Helper
  static inline std::uint8_t pack_piece(bb::Piece p) noexcept;
  static inline bb::Piece unpack_piece(std::uint8_t pp) noexcept;
};

}  // namespace lilia::model
