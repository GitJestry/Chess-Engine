#pragma once
#include "board.hpp"
#include "model_types.hpp"

namespace lilia {

class Evaluation {
 public:
  int evaluate(const Board& board, core::Color sideToMove) const;

 private:
  int material(const Board& board) const;
  int positional(const Board& board) const;
};

}  // namespace lilia
