#include "lilia/engine/lmr_red.hpp"

#include <array>
#include <cmath>

namespace lilia::engine {

consteval LMRTable build_LMR_RED(double base = 0.33, double scale = 3.6) {
  LMRTable table{};
  for (int d = 0; d <= LMR_MAX_D; ++d) {
    for (int m = 0; m <= LMR_MAX_M; ++m) {
      double rd = (d <= 1 || m <= 1)
                      ? 0.0
                      : base + std::log(static_cast<double>(d)) *
                                   std::log(2.0 + static_cast<double>(m)) / scale;
      int r = static_cast<int>(rd);
      if (r < 0) r = 0;
      if (d > 0 && r > d - 1) r = d - 1;
      table[d][m] = r;
    }
  }
  return table;
}

alignas(64) constexpr LMRTable LMR_RED = build_LMR_RED();

}  // namespace lilia::engine

