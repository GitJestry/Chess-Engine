#pragma once
#include <cstddef>
#include <vector>

namespace lilia::engine {

struct TexelSample {
  std::vector<double> features;  // feature vector describing a position
  double target = 0.0;           // Stockfish evaluation in centipawns
};

class TexelTuner {
 public:
  explicit TexelTuner(std::size_t featureCount);

  void addSample(const std::vector<double>& features, double target);
  void tune(std::size_t iterations, double k = 1.0 / 400.0, double lr = 1e-3);

  const std::vector<double>& weights() const noexcept { return m_weights; }

 private:
  std::vector<TexelSample> m_samples;
  std::vector<double> m_weights;
};

}  // namespace lilia::engine

