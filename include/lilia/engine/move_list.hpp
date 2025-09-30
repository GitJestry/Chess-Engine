#pragma once
#include <tuple>
#include <utility>

#include "lilia/model/move.hpp"

namespace lilia::engine {

// descending insertion sort on parallel arrays
template <typename... PayloadArrays>
inline void sort_by_score_desc(int* score, model::Move* moves, int n, PayloadArrays... payload) {
  for (int i = 1; i < n; ++i) {
    int s = score[i];
    model::Move m = moves[i];
    int j = i - 1;
    if constexpr (sizeof...(payload) > 0) {
      auto payloadValues = std::tuple{payload[i]...};
      while (j >= 0 && score[j] < s) {
        score[j + 1] = score[j];
        moves[j + 1] = moves[j];
        ((payload[j + 1] = payload[j]), ...);
        --j;
      }
      score[j + 1] = s;
      moves[j + 1] = m;
      std::apply(
          [&](auto&&... values) {
            ((payload[j + 1] = std::forward<decltype(values)>(values)), ...);
          },
          payloadValues);
    } else {
      while (j >= 0 && score[j] < s) {
        score[j + 1] = score[j];
        moves[j + 1] = moves[j];
        --j;
      }
      score[j + 1] = s;
      moves[j + 1] = m;
    }
  }
}

}  // namespace lilia::engine
