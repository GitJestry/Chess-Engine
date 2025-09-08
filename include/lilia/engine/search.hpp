#pragma once

// -----------------------------------------------------------------------------
// Branch-Prediction Hints
// -----------------------------------------------------------------------------
#if defined(__GNUC__) || defined(__clang__)
#define LILIA_LIKELY(x) __builtin_expect(!!(x), 1)
#define LILIA_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LILIA_LIKELY(x) (x)
#define LILIA_UNLIKELY(x) (x)
#endif

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "../model/move_generator.hpp"
#include "../model/position.hpp"
#include "../model/tt5.hpp"
#include "config.hpp"
#include "eval.hpp"

namespace lilia::engine {

// -----------------------------------------------------------------------------
// Limits / constants
// -----------------------------------------------------------------------------
// Für History-Tabellen: Anzahl nicht-leerer Figurtypen (Pawn..King)
static constexpr int PIECE_NB = 6;
static constexpr int SQ_NB = 64;

struct SearchStoppedException : public std::exception {
  const char* what() const noexcept override { return "Search stopped"; }
};

// -----------------------------------------------------------------------------
// SearchStats – robustere Zähler (64-bit), schlanke Ausgabeinfos
// -----------------------------------------------------------------------------
struct SearchStats {
  std::uint64_t nodes = 0;  // 64-bit: vermeidet Overflow
  double nps = 0.0;
  std::uint64_t elapsedMs = 0;
  int bestScore = 0;
  std::optional<model::Move> bestMove;
  std::vector<std::pair<model::Move, int>> topMoves;
  std::vector<model::Move> bestPV;
};

// Vorwärtsdeklaration
class Evaluator;

// -----------------------------------------------------------------------------
// Search – ein Instanz-pro-Thread (keine geteilten mutablen Daten)
// -----------------------------------------------------------------------------
class Search {
 public:
  Search(model::TT5& tt, std::shared_ptr<const Evaluator> eval, const EngineConfig& cfg);
  ~Search() = default;

  // Non-copyable / non-movable – bewusst, um versehentliches Kopieren zu verhindern
  Search(const Search&) = delete;
  Search& operator=(const Search&) = delete;
  Search(Search&&) = delete;
  Search& operator=(Search&&) = delete;

  // Root (iterative deepening, parallel auf Root-Children)
  // maxThreads <= 0 -> use cfg.threads for deterministic thread count
  int search_root_parallel(model::Position& pos, int depth, std::shared_ptr<std::atomic<bool>> stop,
                           int maxThreads = 0, std::uint64_t maxNodes = 0);
  void set_node_limit(std::shared_ptr<std::atomic<std::uint64_t>> shared, std::uint64_t limit) {
    sharedNodes = std::move(shared);
    nodeLimit = limit;
  }

  [[nodiscard]] const SearchStats& getStats() const noexcept { return stats; }
  void clearSearchState();  // Killers/History resetten

  model::TT5& ttRef() noexcept { return tt; }

  // Killers: 2 je Ply
  alignas(64) std::array<std::array<model::Move, 2>, MAX_PLY> killers{};

  // Basishistory (von->nach), bewährt und billig
  alignas(64) std::array<std::array<int16_t, SQ_NB>, SQ_NB> history{};

  // Erweiterte Heuristiken (für bessere Move-Order/Cutoffs)
  // Quiet-History: nach (moverPiece, to)
  alignas(64) int16_t quietHist[PIECE_NB][SQ_NB] = {};

  // Capture-History: nach (moverPiece, to, capturedPiece)
  alignas(64) int16_t captureHist[PIECE_NB][SQ_NB][PIECE_NB] = {};

  // Continuation history: (prev mover piece, prev to) × (from,to)
  alignas(64) int16_t contHist[PIECE_NB][SQ_NB][SQ_NB][SQ_NB] = {};

  // Counter-Move: nach vorigem Zug (from,to) → typischer Antwortzug,
  // plus Counter-History-Bonus für genau diesen Antwortzug
  alignas(64) model::Move counterMove[SQ_NB][SQ_NB] = {};
  alignas(64) int16_t counterHist[SQ_NB][SQ_NB] = {};

 private:
  // Kernfunktionen
  int negamax(model::Position& pos, int depth, int alpha, int beta, int ply, model::Move& refBest,
              int parentStaticEval = 0, const model::Move* excludedMove = nullptr);
  int quiescence(model::Position& pos, int alpha, int beta, int ply);
  std::vector<model::Move> build_pv_from_tt(model::Position pos, int max_len = 16);
  int signed_eval(model::Position& pos);
  // Copy global heuristics into this worker (killers are reset, on purpose)
  void copy_heuristics_from(const Search& src);
  // Merge this worker's heuristics into the global (killers are NOT merged)
  void merge_from(const Search& other);

  uint32_t tick_ = 0;
  static constexpr uint32_t TICK_STEP = 1024;  // 256–2048 ist ok

  inline void fast_tick() {
    // billiger Hot-Path
    ++tick_;
    if ((tick_ & (TICK_STEP - 1)) != 0) return;

    // seltener Slow-Path
    if (sharedNodes) {
      auto cur = sharedNodes->fetch_add(TICK_STEP, std::memory_order_relaxed) + TICK_STEP;
      if (nodeLimit && cur >= nodeLimit) {
        if (stopFlag) stopFlag->store(true, std::memory_order_relaxed);
        throw SearchStoppedException();
      }
    }
    if (stopFlag && stopFlag->load(std::memory_order_relaxed)) throw SearchStoppedException();
  }

  // optional: damit die letzten <TICK_STEP Restknoten noch gezählt werden
  inline void flush_tick() {
    if (!sharedNodes) return;
    uint32_t rem = (tick_ & (TICK_STEP - 1));
    if (rem) sharedNodes->fetch_add(rem, std::memory_order_relaxed);
  }

  // ---------------------------------------------------------------------------
  // Daten
  // ---------------------------------------------------------------------------
  model::TT5& tt;
  model::MoveGenerator mg;
  const EngineConfig& cfg;
  std::shared_ptr<const Evaluator> eval_;

  // Voriger Zug pro Ply (für CounterMove)
  std::array<model::Move, MAX_PLY> prevMove{};

  // Feste, flache Puffer pro Ply:
  model::Move genArr_[MAX_PLY][lilia::engine::MAX_MOVES];
  int genN_[MAX_PLY]{};

  model::Move capArr_[MAX_PLY][lilia::engine::MAX_MOVES];
  int capN_[MAX_PLY]{};

  // Stop/Stats
  std::shared_ptr<std::atomic<bool>> stopFlag;
  SearchStats stats;
  std::shared_ptr<std::atomic<std::uint64_t>> sharedNodes;
  std::uint64_t nodeLimit = 0;
};

}  // namespace lilia::engine
