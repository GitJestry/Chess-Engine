#pragma once
#include <cstdint>

#include "../chess_types.hpp"

// =======================================================
// Bitboard Layout in Stockfish-Convention:
// - Bit 0 (LSB)   = a1 (bottom left white view)
// - Bit 63 (MSB)  = h8 (top right white view)
// This File defines:
// - Square-Enum for every square
// - helpfunctions and masks for Files und Ranks
// =======================================================

namespace lilia {
namespace core {
using Bitboard = uint64_t;
// Returns a bitboard with only the given square set
[[nodiscard]] constexpr inline Bitboard square_bb(Square sq) noexcept {
  return 1ULL << static_cast<int>(sq);
}

// Masks for single Files
constexpr Bitboard FILE_A = 0x0101010101010101ULL;
constexpr Bitboard FILE_B = FILE_A << 1;
constexpr Bitboard FILE_C = FILE_A << 2;
constexpr Bitboard FILE_D = FILE_A << 3;
constexpr Bitboard FILE_E = FILE_A << 4;
constexpr Bitboard FILE_F = FILE_A << 5;
constexpr Bitboard FILE_G = FILE_A << 6;
constexpr Bitboard FILE_H = FILE_A << 7;

// Masks for single ranks
constexpr Bitboard RANK_1 = 0x00000000000000FFULL;
constexpr Bitboard RANK_2 = RANK_1 << 8;
constexpr Bitboard RANK_3 = RANK_1 << 16;
constexpr Bitboard RANK_4 = RANK_1 << 24;
constexpr Bitboard RANK_5 = RANK_1 << 32;
constexpr Bitboard RANK_6 = RANK_1 << 40;
constexpr Bitboard RANK_7 = RANK_1 << 48;
constexpr Bitboard RANK_8 = RANK_1 << 56;

[[nodiscard]] constexpr inline int file_of(Square sq) noexcept {
  return static_cast<int>(sq) % 8;
}

[[nodiscard]] constexpr inline int rank_of(Square sq) noexcept {
  return static_cast<int>(sq) / 8;
}
}  // namespace core
}  // namespace lilia
