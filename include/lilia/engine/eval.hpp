#pragma once
#include <cstdint>
#include <memory>

namespace lilia {
namespace model {
class Position;
}

namespace engine {

class Evaluator final {
 public:
  Evaluator() noexcept;
  ~Evaluator() noexcept;

  // Bewertung in cp aus Sicht der Seite am Zug.
  int evaluate(model::Position& pos) const;

  // Eval- & Pawn-Caches leeren.
  void clearCaches() const noexcept;

  Evaluator(const Evaluator&) = delete;
  Evaluator& operator=(const Evaluator&) = delete;
  Evaluator(Evaluator&&) = delete;
  Evaluator& operator=(Evaluator&&) = delete;

 private:
  struct Impl;
  mutable std::unique_ptr<Impl> m_impl;
};

}  // namespace engine
}  // namespace lilia
