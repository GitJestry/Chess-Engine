#pragma once
#include <array>
#include <optional>

#include "core/model_types.hpp"

namespace lilia::model {

class Board {
 public:
  Board();

  void clear();

  // API unverändert
  void setPiece(core::Square sq, bb::Piece p);
  void removePiece(core::Square sq);
  std::optional<bb::Piece> getPiece(core::Square sq) const;

  bb::Bitboard getPieces(core::Color c) const { return m_color_occ[bb::ci(c)]; }
  bb::Bitboard getAllPieces() const { return m_all_occ; }
  bb::Bitboard getPieces(core::Color c, core::PieceType t) const {
    return m_bb[bb::ci(c)][static_cast<int>(t)];
  }

  // Optionaler Fast-Path (später in Position nutzen):
  // bewegt eine Figur von 'from' nach 'to' (ohne Capture; 'to' muss leer sein)
  void movePiece_noCapture(core::Square from, core::Square to);

 private:
  // Bitboards wie gehabt
  std::array<std::array<bb::Bitboard, 6>, 2> m_bb{};
  std::array<bb::Bitboard, 2> m_color_occ{};
  bb::Bitboard m_all_occ = 0;

  // Neu: O(1)-Lookup pro Feld (0 = leer, sonst (ptIdx+1) | (color<<3))
  std::array<std::uint8_t, 64> m_piece_on{};

  // Helper
  static inline int type_index(core::PieceType t);
  static inline std::uint8_t pack_piece(bb::Piece p);
  static inline bb::Piece unpack_piece(std::uint8_t pp);
};

}  // namespace lilia::model
