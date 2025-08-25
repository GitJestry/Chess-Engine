#pragma once
#include "../model/position.hpp"

namespace lilia::engine {

class Evaluator {
 public:
  Evaluator();
  int evaluate(model::Position& pos) const;  // centipawns: White positive
 private:
  int pst[2][6][64];
};

}  // namespace lilia
