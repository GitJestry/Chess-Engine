#pragma once
#include "../model/position.hpp"

namespace lilia {

class Evaluator {
 public:
  Evaluator();
  int evaluate(const model::Position& pos) const;  // centipawns: White positive
 private:
  int pst[2][6][64];
};

}  // namespace lilia
