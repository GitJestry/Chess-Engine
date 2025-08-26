#pragma once
#include <array>
#include <cstdint>

#include "core/model_types.hpp"

namespace lilia::model {

struct Zobrist {
  static inline std::array<std::array<std::array<bb::Bitboard, 64>, 6>, 2> piece{};

  static inline std::array<bb::Bitboard, 16> castling{};

  static inline std::array<bb::Bitboard, 8> epFile{};

  static inline bb::Bitboard side = 0;

  static void init(bb::Bitboard seed = 0x9E3779B97F4A7C15ULL);

  template <class PositionLike>
  static bb::Bitboard compute(const PositionLike& pos);
};

}  // namespace lilia::model
