#include "lilia/model/board.hpp"

#include <cassert>

namespace lilia::model {

static inline int type_index_impl(core::PieceType t) {
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

// ---- Board ----

Board::Board() {
  clear();
}

void Board::clear() {
  for (auto& byColor : m_bb) byColor.fill(0);
  m_color_occ = {0, 0};
  m_all_occ = 0;
  m_piece_on.fill(0);
}

inline int Board::type_index(core::PieceType t) {
  return type_index_impl(t);
}

inline std::uint8_t Board::pack_piece(bb::Piece p) {
  if (p.type == core::PieceType::None) return 0;
  const int ti = type_index_impl(p.type);                 // 0..5
  const std::uint8_t c = (bb::ci(p.color) & 1u);          // 0=white,1=black
  return static_cast<std::uint8_t>((ti + 1) | (c << 3));  // low3: (ti+1), bit3: color
}

inline bb::Piece Board::unpack_piece(std::uint8_t pp) {
  if (pp == 0) return bb::Piece{core::PieceType::None, core::Color::White};
  const int ti = (pp & 0x7) - 1;  // 0..5
  const core::PieceType pt = static_cast<core::PieceType>(ti);
  const core::Color col = ((pp >> 3) & 1u) ? core::Color::Black : core::Color::White;
  return bb::Piece{pt, col};
}

void Board::setPiece(core::Square sq, bb::Piece p) {
  // remove falls etwas steht
  removePiece(sq);
  if (p.type == core::PieceType::None) return;

  const int ti = type_index(p.type);
  assert(ti >= 0 && "Invalid PieceType");

  const int ci = bb::ci(p.color);
  const bb::Bitboard mask = bb::sq_bb(sq);

  // Bitboards + Occupancies inkrementell
  m_bb[ci][ti] |= mask;
  m_color_occ[ci] |= mask;
  m_all_occ |= mask;

  // by-square
  m_piece_on[static_cast<int>(sq)] = pack_piece(p);
}

void Board::removePiece(core::Square sq) {
  const int s = static_cast<int>(sq);
  const std::uint8_t packed = m_piece_on[s];
  if (!packed) return;  // war leer

  const bb::Piece p = unpack_piece(packed);
  const int ti = type_index(p.type);
  const int ci = bb::ci(p.color);
  const bb::Bitboard mask = bb::sq_bb(sq);

  // Bitboards + Occupancies inkrementell
  m_bb[ci][ti] &= ~mask;
  m_color_occ[ci] &= ~mask;
  m_all_occ &= ~mask;

  // by-square
  m_piece_on[s] = 0;
}

std::optional<bb::Piece> Board::getPiece(core::Square sq) const {
  const std::uint8_t packed = m_piece_on[static_cast<int>(sq)];
  if (!packed) return std::nullopt;
  return unpack_piece(packed);
}

void Board::movePiece_noCapture(core::Square from, core::Square to) {
  const int sf = static_cast<int>(from);
  const int st = static_cast<int>(to);
  const std::uint8_t packed = m_piece_on[sf];
  if (!packed) return;  // nichts zu bewegen
  assert(m_piece_on[st] == 0 && "movePiece_noCapture: 'to' must be empty");

  const bb::Piece p = unpack_piece(packed);
  const int ti = type_index(p.type);
  const int ci = bb::ci(p.color);

  const bb::Bitboard fromMask = bb::sq_bb(from);
  const bb::Bitboard toMask = bb::sq_bb(to);

  // Bitboards
  m_bb[ci][ti] &= ~fromMask;
  m_bb[ci][ti] |= toMask;

  // Occupancies
  m_color_occ[ci] ^= fromMask;  // remove from
  m_color_occ[ci] |= toMask;    // add to
  m_all_occ ^= fromMask;
  m_all_occ |= toMask;

  // by-square
  m_piece_on[sf] = 0;
  m_piece_on[st] = packed;
}

}  // namespace lilia::model
