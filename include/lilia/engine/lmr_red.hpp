#pragma once
#include <array>
#include <cstddef>

namespace lilia::engine {

constexpr int LMR_MAX_D = 64;  // depth index [0..64]
constexpr int LMR_MAX_M = 64;  // move-number index [0..64]

using LMRTable = std::array<std::array<int, LMR_MAX_M + 1>, LMR_MAX_D + 1>;

extern const LMRTable LMR_RED;

// Returns the precomputed Late Move Reduction for a given depth and move number.
constexpr int lmr_red(int depth, int move) {
  return LMR_RED[depth][move];
}

}  // namespace lilia::engine
