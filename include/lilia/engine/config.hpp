#pragma once
#include <cstddef>

namespace lilia {
struct EngineConfig {
  int maxDepth = 10;
  std::size_t ttSizeMb = 64;
  bool useNullMove = true;
  bool useLMR = true;
  bool useAspiration = true;
  int aspirationWindow = 50;  // centipawns
};

static const int base_value[6] = {100, 320, 330, 500, 900, 20000};
}  // namespace lilia
