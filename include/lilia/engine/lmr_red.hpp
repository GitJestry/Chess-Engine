#pragma once
#include <cstddef>

namespace lilia::engine {

constexpr int LMR_MAX_D = 64;  // depth index [0..64]
constexpr int LMR_MAX_M = 64;  // move-number index [0..64]

// Late Move Reduction lookup table: reductions in plies.
// Access like: LMR_RED[depth][moveNumber]
extern int LMR_RED[LMR_MAX_D + 1][LMR_MAX_M + 1];

// Build (or rebuild) the table. Call with your own tuning if desired.
// base: additive bias; scale: overall reduction strength.
// Typical good defaults: base=0.33, scale=3.6
void build_LMR_RED(double base = 0.33, double scale = 3.6);

}  // namespace lilia::engine
