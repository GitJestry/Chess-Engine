#pragma once
#include "lilia/model/move.hpp"

namespace lilia::engine {

struct MoveArray {
  model::Move mv[256];
  int sz = 0;
  inline void clear() { sz = 0; }
  inline void push(const model::Move& m) { mv[sz++] = m; }
  inline int size() const { return sz; }
  inline const model::Move& operator[](int i) const { return mv[i]; }
  inline model::Move& operator[](int i) { return mv[i]; }
};

// descending insertion sort on parallel arrays
inline void sort_by_score_desc(int* score, model::Move* moves, int n) {
  for (int i = 1; i < n; ++i) {
    int s = score[i];
    model::Move m = moves[i];
    int j = i - 1;
    while (j >= 0 && score[j] < s) {
      score[j + 1] = score[j];
      moves[j + 1] = moves[j];
      --j;
    }
    score[j + 1] = s;
    moves[j + 1] = m;
  }
}

}  // namespace lilia::engine
