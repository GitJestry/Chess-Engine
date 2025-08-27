#pragma once
#include "../../chess_types.hpp"

namespace lilia::model::bb {
using Bitboard = std::uint64_t;

struct Piece {
  core::PieceType type = core::PieceType::None;
  core::Color color = core::Color::White;
  constexpr bool isNone() const { return type == core::PieceType::None; }
};
constexpr inline int ci(core::Color c) {
  return c == core::Color::White ? 0 : 1;
}

constexpr inline int file_of(core::Square s) {
  return s & 7;
}
constexpr inline int rank_of(core::Square s) {
  return s >> 3;
}
constexpr inline Bitboard sq_bb(core::Square s) {
  return Bitboard{1} << s;
}

constexpr Bitboard FILE_A = 0x0101010101010101ULL;
constexpr Bitboard FILE_B = 0x0202020202020202ULL;
constexpr Bitboard FILE_G = 0x4040404040404040ULL;
constexpr Bitboard FILE_H = 0x8080808080808080ULL;

constexpr Bitboard RANK_1 = 0x00000000000000FFULL;
constexpr Bitboard RANK_2 = 0x000000000000FF00ULL;
constexpr Bitboard RANK_3 = 0x0000000000FF0000ULL;
constexpr Bitboard RANK_4 = 0x00000000FF000000ULL;
constexpr Bitboard RANK_5 = 0x000000FF00000000ULL;
constexpr Bitboard RANK_6 = 0x0000FF0000000000ULL;
constexpr Bitboard RANK_7 = 0x00FF000000000000ULL;
constexpr Bitboard RANK_8 = 0xFF00000000000000ULL;

constexpr core::Square A1 = 0, D1 = 3, E1 = 4, F1 = 5, H1 = 7;
constexpr core::Square A8 = 56, D8 = 59, E8 = 60, F8 = 61, H8 = 63;

enum Castling : std::uint8_t { WK = 1 << 0, WQ = 1 << 1, BK = 1 << 2, BQ = 1 << 3 };

}  // namespace lilia::model::bb
