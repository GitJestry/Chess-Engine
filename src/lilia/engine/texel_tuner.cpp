#include "lilia/engine/texel_tuner.hpp"

#include <cmath>

namespace lilia::engine {

TexelTuner::TexelTuner(std::size_t featureCount)
    : m_weights(featureCount, 0.0) {}

void TexelTuner::addSample(const std::vector<double>& features, double target) {
  m_samples.push_back({features, target});
}

void TexelTuner::tune(std::size_t iterations, double k, double lr) {
  for (std::size_t it = 0; it < iterations; ++it) {
    for (const auto& s : m_samples) {
      double eval = 0.0;
      for (std::size_t i = 0; i < m_weights.size(); ++i)
        eval += m_weights[i] * s.features[i];

      const double p = 1.0 / (1.0 + std::exp(-k * eval));
      const double t = 1.0 / (1.0 + std::exp(-k * s.target));
      const double diff = p - t;

      for (std::size_t i = 0; i < m_weights.size(); ++i) {
        m_weights[i] -= lr * diff * k * s.features[i];
      }
    }
  }
}

}  // namespace lilia::engine

