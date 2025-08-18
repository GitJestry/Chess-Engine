#pragma once
#include <array>
#include <cstdint>

#include "core/model_types.hpp"

namespace lilia::model {

struct Zobrist {
  // piece[color][pieceType(0..5)][square]
  static inline std::array<std::array<std::array<bb::Bitboard, 64>, 6>, 2> piece{};
  // castling rights 0..15 (WK|WQ|BK|BQ bitmask)
  static inline std::array<bb::Bitboard, 16> castling{};
  // en-passant file 0..7 (only file is hashed, like Stockfish) ; 8 means "none" → not hashed
  static inline std::array<bb::Bitboard, 8> epFile{};
  // side to move
  static inline bb::Bitboard side = 0;

  static void init(bb::Bitboard seed = 0x9E3779B97F4A7C15ULL);

  // Compute full hash from a position (for verification or initial build)
  template <class PositionLike>
  static bb::Bitboard compute(const PositionLike& pos);
};

}  // namespace lilia::model
