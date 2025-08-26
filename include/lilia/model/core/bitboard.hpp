#pragma once
#include <bit>
#include <cassert>
#include <iostream>

#include "model_types.hpp"

namespace lilia::model::bb {

constexpr inline bool any(Bitboard b) {
  return b != 0;
}
constexpr inline bool none(Bitboard b) {
  return b == 0;
}

constexpr inline int popcount(Bitboard b) {
  return std::popcount(b);
}
inline int ctz64(uint64_t x) noexcept {
  return static_cast<int>(std::countr_zero(x));
}

inline core::Square pop_lsb(Bitboard& b) {
  if (b == 0) return core::NO_SQUARE;
  int index = ctz64(b);
  b &= b - 1;
  return static_cast<core::Square>(index);
}

constexpr inline Bitboard north(Bitboard b) {
  return b << 8;
}
constexpr inline Bitboard south(Bitboard b) {
  return b >> 8;
}
constexpr inline Bitboard east(Bitboard b) {
  return (b & ~FILE_H) << 1;
}
constexpr inline Bitboard west(Bitboard b) {
  return (b & ~FILE_A) >> 1;
}
constexpr inline Bitboard ne(Bitboard b) {
  return (b & ~FILE_H) << 9;
}
constexpr inline Bitboard nw(Bitboard b) {
  return (b & ~FILE_A) << 7;
}
constexpr inline Bitboard se(Bitboard b) {
  return (b & ~FILE_H) >> 7;
}
constexpr inline Bitboard sw(Bitboard b) {
  return (b & ~FILE_A) >> 9;
}

constexpr inline Bitboard knight_attacks_from(core::Square s) {
  Bitboard b = sq_bb(s);
  Bitboard l1 = (b & ~FILE_A) >> 1, l2 = (b & ~(FILE_A | FILE_B)) >> 2;
  Bitboard r1 = (b & ~FILE_H) << 1, r2 = (b & ~(FILE_H | FILE_G)) << 2;
  return (l2 << 8) | (l2 >> 8) | (r2 << 8) | (r2 >> 8) | (l1 << 16) | (l1 >> 16) | (r1 << 16) |
         (r1 >> 16);
}

constexpr inline Bitboard king_attacks_from(core::Square s) {
  Bitboard b = sq_bb(s);
  return east(b) | west(b) | north(b) | south(b) | ne(b) | nw(b) | se(b) | sw(b);
}

inline Bitboard ray_attack_dir(Bitboard from, Bitboard occ, Bitboard (*step)(Bitboard)) {
  Bitboard atk = 0, r = step(from);
  while (r) {
    atk |= r;
    if (r & occ) break;
    r = step(r);
  }
  return atk;
}

inline Bitboard bishop_attacks(core::Square s, Bitboard occ) {
  Bitboard from = sq_bb(s);
  return ray_attack_dir(from, occ, ne) | ray_attack_dir(from, occ, nw) |
         ray_attack_dir(from, occ, se) | ray_attack_dir(from, occ, sw);
}

inline Bitboard rook_attacks(core::Square s, Bitboard occ) {
  Bitboard from = sq_bb(s);
  return ray_attack_dir(from, occ, north) | ray_attack_dir(from, occ, south) |
         ray_attack_dir(from, occ, east) | ray_attack_dir(from, occ, west);
}

inline Bitboard queen_attacks(core::Square s, Bitboard occ) {
  return bishop_attacks(s, occ) | rook_attacks(s, occ);
}

constexpr inline Bitboard white_pawn_attacks(Bitboard pawns) {
  return (nw(pawns) | ne(pawns));
}
constexpr inline Bitboard black_pawn_attacks(Bitboard pawns) {
  return (sw(pawns) | se(pawns));
}

}  // namespace lilia::model::bb
