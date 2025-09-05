#include "lilia/engine/lmr_red.hpp"

#include <array>
#include <limits>  // optional

namespace lilia::engine {

// Fast enough for table building; ~1e-12 abs error for typical inputs used here.
constexpr double ct_log(double x) {
  // Domain guard for safety; you can also assert if you prefer.
  if (!(x > 0.0)) return -std::numeric_limits<double>::infinity();

  const double y = (x - 1.0) / (x + 1.0);
  const double y2 = y * y;
  double term = y;
  double sum = 0.0;
  // 25 terms is plenty for our inputs; tweak if you like.
  for (int n = 1; n <= 49; n += 2) {
    sum += term / static_cast<double>(n);
    term *= y2;
  }
  return 2.0 * sum;
}

consteval LMRTable build_LMR_RED(double base = 0.33, double scale = 3.6) {
  LMRTable table{};
  for (int d = 0; d <= LMR_MAX_D; ++d) {
    for (int m = 0; m <= LMR_MAX_M; ++m) {
      double rd = (d <= 1 || m <= 1) ? 0.0
                                     : base + ct_log(static_cast<double>(d)) *
                                                  ct_log(2.0 + static_cast<double>(m)) / scale;
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
