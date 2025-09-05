#pragma once
#include <cstddef>

namespace lilia::engine {
struct EngineConfig {
  int maxDepth = 12;  // etwas tiefer, ID hilft Stabilität
  std::uint64_t maxNodes = 100000;
  std::size_t ttSizeMb = 1024;  // mehr TT entspannt Aspiration/Transpositionen
  bool useNullMove = true;      // gut für Mittelspiel, QS-Fixes mindern Risiken
  bool useLMR = true;           // leichte Reduktionen sind ok
  bool useAspiration = true;    // stabil mit Score-Normalisierung
  int aspirationWindow = 20;    // nicht zu eng, sonst Re-Search-Flapping
  int threads = 0;              // 0 => auto(HW); Engine begrenzt ohnehin
  bool useLMP = true;           // Late Move Pruning (quiet, flach)
  bool useIID = true;
  bool useSingular = true;
  int lmpDepthMax = 3;  // nur für Tiefe <= 3
  int lmpBase = 2;      // Schwelle ~ lmpBase + depth*depth

  bool useFutility = true;  // Futility bei depth==1, quiet
  int futilityMargin = 125;

  bool useReverseFutility = true;  // flach: staticEval >> beta = Cut, nicht im Schach
  bool useSEEPruning = true;       // schlechte Captures früh kappen (qsearch/low depth)
  bool useProbCut = true;
  bool qsearchQuietChecks = true;

  // LMR-Feintuning
  int lmrBase = 1;            // Grundreduktion
  int lmrMax = 3;             // Deckel
  bool lmrUseHistory = true;  // gute History => weniger Reduktion
};
static const int base_value[6] = {100, 320, 330, 500, 900, 20000};
constexpr int INF = 32000;
constexpr int MATE = 30000;
constexpr int MAX_PLY = 128;
constexpr int MATE_THR = MATE - 512;        // Mate threshold for detection/encoding
static constexpr int VALUE_INF = MATE - 1;  // nie größer als Mate!
}  // namespace lilia::engine
