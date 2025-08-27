#pragma once
#include <cstddef>

namespace lilia::engine {
struct EngineConfig {
  int maxDepth = 12;           // etwas tiefer, ID hilft Stabilität
  std::size_t ttSizeMb = 512;  // mehr TT entspannt Aspiration/Transpositionen
  bool useNullMove = true;     // gut für Mittelspiel, QS-Fixes mindern Risiken
  bool useLMR = true;          // leichte Reduktionen sind ok
  bool useAspiration = true;   // stabil mit Score-Normalisierung
  int aspirationWindow = 40;   // nicht zu eng, sonst Re-Search-Flapping
  int threads = 0;             // 0 => auto(HW); Engine begrenzt ohnehin
};
static const int base_value[6] = {100, 320, 330, 500, 900, 20000};
constexpr int INF = 30000;
constexpr int MATE = 32000;
}  // namespace lilia::engine
