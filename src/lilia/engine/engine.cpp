#include "lilia/engine/engine.hpp"

#include <algorithm>
#include <thread>  // hardware_concurrency

#include "lilia/engine/eval.hpp"  // <- Evaluator
#include "lilia/engine/search.hpp"
#include "lilia/model/core/magic.hpp"

namespace lilia::engine {

struct Engine::Impl {
  EngineConfig cfg;
  model::TT4 tt;

  // Gemeinsame Evaluator-Instanz, von allen Searches/Threads genutzt
  std::shared_ptr<const Evaluator> eval;
  std::unique_ptr<Search> search;

  explicit Impl(const EngineConfig& c) : cfg(c), tt(c.ttSizeMb) {
    // Threads: standardmäßig (logical - 1), mind. 1
    unsigned int hw = std::thread::hardware_concurrency();
    int logical = (hw > 0 ? static_cast<int>(hw) : 1);
    cfg.threads = std::max(1, logical - 1);

    // Eine Evaluator-Instanz für die gesamte Engine
    // (falls dein Evaluator einen speziellen Ctor braucht, hier entsprechend anpassen)
    eval = std::make_shared<Evaluator>();

    // Search nutzt dieselbe TT und teilt sich den Evaluator via shared_ptr
    search = std::make_unique<Search>(tt, eval, cfg);
  }
};

Engine::Engine(const EngineConfig& cfg) : pimpl(new Impl(cfg)) {}

Engine::~Engine() {
  delete pimpl;
}

std::optional<model::Move> Engine::find_best_move(model::Position& pos, int maxDepth,
                                                  std::shared_ptr<std::atomic<bool>> stop) {
  if (maxDepth <= 0) maxDepth = pimpl->cfg.maxDepth;

  // Root-Search (parallel)
  pimpl->search->search_root_parallel(pos, maxDepth, std::move(stop), pimpl->cfg.threads);

  // BestMove aus den Stats zurückgeben
  return pimpl->search->getStats().bestMove;
}

const SearchStats& Engine::getLastSearchStats() const {
  return pimpl->search->getStats();
}

const EngineConfig& Engine::getConfig() const {
  return pimpl->cfg;
}

}  // namespace lilia::engine
