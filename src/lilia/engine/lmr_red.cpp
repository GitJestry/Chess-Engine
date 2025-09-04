#include "lilia/engine/lmr_red.hpp"

#include <cmath>  // std::log
#include <mutex>  // optional if you call build from multiple threads

namespace lilia::engine {

alignas(64) int LMR_RED[LMR_MAX_D + 1][LMR_MAX_M + 1];

// Fill table with safe, monotone, bounded reductions.
// r = floor(base + log(depth) * log(2 + move) / scale)
// Clamped to [0, depth-1] and 0 for tiny depth/move.
void build_LMR_RED(double base, double scale) {
  for (int d = 0; d <= LMR_MAX_D; ++d) {
    for (int m = 0; m <= LMR_MAX_M; ++m) {
      double rd = (d <= 1 || m <= 1) ? 0.0
                                     : base + std::log(static_cast<double>(d)) *
                                                  std::log(2.0 + static_cast<double>(m)) / scale;
      int r = static_cast<int>(rd);
      if (r < 0) r = 0;
      if (d > 0 && r > d - 1) r = d - 1;
      LMR_RED[d][m] = r;
    }
  }
}

// Auto-build with defaults at program start.
// If you want different coefficients, call build_LMR_RED() once
// early in your init (before starting the search threads) to overwrite.
struct LMRInitOnce {
  LMRInitOnce() { build_LMR_RED(); }
} static _lmrInitOnce;

}  // namespace lilia::engine
