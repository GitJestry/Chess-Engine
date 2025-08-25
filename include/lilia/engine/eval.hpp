#pragma once
#include <cstdint>

namespace lilia {
namespace model {
class Position;
}  // namespace model
namespace engine {

class Evaluator {
 public:
  Evaluator() noexcept;
  ~Evaluator() noexcept;

  int evaluate(model::Position& pos) const;
  void clearCaches() const noexcept;

 private:
  struct Impl;
  mutable Impl* m_impl = nullptr;  // mutable: caches are modified even in const evaluate()
};

}  // namespace engine
}  // namespace lilia
