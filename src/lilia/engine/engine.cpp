#include "lilia/engine/engine.hpp"

#include <algorithm>
#include <thread>  // hardware_concurrency

#include "lilia/engine/eval.hpp"  // <- Evaluator
#include "lilia/engine/move_order.hpp"
#include "lilia/engine/search.hpp"
#include "lilia/engine/thread_pool.hpp"
#include "lilia/model/core/magic.hpp"

namespace lilia::engine {

struct Engine::Impl {
  EngineConfig cfg;
  model::TT5 tt;

  // Gemeinsame Evaluator-Instanz, von allen Searches/Threads genutzt
  std::shared_ptr<const Evaluator> eval;
  std::unique_ptr<Search> search;

  explicit Impl(const EngineConfig& c) : cfg(c), tt(c.ttSizeMb) {
    unsigned hw = std::thread::hardware_concurrency();
    int logical = (hw > 0 ? (int)hw : 1);

    if (cfg.threads <= 0) {
      cfg.threads = std::max(1, logical - 1);  // auto
    } else {
      cfg.threads = std::clamp(cfg.threads, 1, logical);
    }

    // Initialize thread pool once using the configured thread count
    ThreadPool::instance(cfg.threads);

    eval = std::make_shared<Evaluator>();
    search = std::make_unique<Search>(tt, eval, cfg);
  }
};

Engine::Engine(const EngineConfig& cfg) : pimpl(new Impl(cfg)) {
  Engine::init();
}

Engine::~Engine() {
  try {
    pimpl->tt.clear();
  } catch (...) {
  }
  try {
    if (pimpl->eval) pimpl->eval->clearCaches();
  } catch (...) {
  }
  try {
    if (pimpl->search) pimpl->search->clearSearchState();
  } catch (...) {
  }
  delete pimpl;
}

std::optional<model::Move> Engine::find_best_move(model::Position& pos, int maxDepth,
                                                  std::shared_ptr<std::atomic<bool>> stop) {
  if (maxDepth <= 0) maxDepth = pimpl->cfg.maxDepth;

  // Suche immer sauber zurücksetzen (Killers/History etc.)
  try {
    pimpl->search->clearSearchState();
  } catch (...) {
  }

  // 1) Suche ausführen – niemals Exceptions nach außen lassen
  try {
    (void)pimpl->search->search_root_lazy_smp(pos, maxDepth, stop, pimpl->cfg.threads
                                              /*,pimpl->cfg.maxNodes*/);
  } catch (...) {
    // Wir fallen gleich auf TT/Legal zurück; keine Weitergabe
  }

  // 2) BestMove aus Stats, wenn vorhanden
  const auto& stats = pimpl->search->getStats();
  if (stats.bestMove.has_value()) return stats.bestMove;  // safe

  // 3) TT-Fallback: am Root-Hash nachsehen und legalisieren
  try {
    auto& tt = pimpl->search->ttRef();
    if (auto e = tt.probe(pos.hash())) {
      model::Move ttMove = e->best;
      if (ttMove.from() >= 0 && ttMove.to() >= 0) {
        model::Position tmp = pos;
        if (tmp.doMove(ttMove))  // legal?
          return ttMove;
      }
    }
  } catch (...) {
    // TT defekt/leer: ignorieren
  }

  // 4) Letzter Fallback: generiere legale Züge und wähle eine vernünftige Heuristik
  try {
    model::MoveGenerator mg;
    std::vector<model::Move> pseudo;
    pseudo.reserve(128);
    mg.generatePseudoLegalMoves(pos.getBoard(), pos.getState(), pseudo);

    // Legal filtern und „gute“ Züge bevorzugen (Captures/Promos via MVV-LVA)
    std::optional<model::Move> bestCapPromo;
    int bestCapScore = std::numeric_limits<int>::min();
    std::optional<model::Move> firstLegal;

    for (auto& m : pseudo) {
      model::Position tmp = pos;
      if (!tmp.doMove(m)) continue;  // illegal -> raus

      if (m.isCapture() || m.promotion() != core::PieceType::None) {
        int sc = mvv_lva_fast(pos, m);  // vorhandene Heuristik
        if (!bestCapPromo || sc > bestCapScore) {
          bestCapPromo = m;
          bestCapScore = sc;
        }
      } else if (!firstLegal) {
        firstLegal = m;  // merken als "zur Not"
      }
    }

    if (bestCapPromo) return bestCapPromo;
    if (firstLegal) return firstLegal;
  } catch (...) {
    // Movegen kaputt? Dann gibt es wirklich nichts zu ziehen.
  }

  // 5) Keine Züge -> Matt/Pat, gib leer zurück (sicher! kein value()-Crash)
  return std::nullopt;
}

const SearchStats& Engine::getLastSearchStats() const {
  return pimpl->search->getStats();
}

const EngineConfig& Engine::getConfig() const {
  return pimpl->cfg;
}

}  // namespace lilia::engine
