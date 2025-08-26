#include "lilia/engine/engine.hpp"

#include <algorithm>

#include "lilia/engine/search.hpp"
#include "lilia/model/core/magic.hpp"

namespace lilia::engine {

struct Engine::Impl {
  EngineConfig cfg;
  model::TT4 tt;
  std::unique_ptr<Search> search;

  Impl(const EngineConfig& c) : cfg(c), tt(c.ttSizeMb) {
    unsigned int hw = std::thread::hardware_concurrency();
    int logical = (hw > 0 ? (int)hw : 1);
    cfg.threads = std::max(1, logical - 1);

    auto evalFactory = []() { return std::make_unique<Evaluator>(); };

    search = std::make_unique<Search>(tt, evalFactory, cfg);
  }
};

Engine::Engine(const EngineConfig& cfg) : pimpl(new Impl(cfg)) {}
Engine::~Engine() {
  delete pimpl;
}

std::optional<model::Move> Engine::find_best_move(model::Position& pos, int maxDepth,
                                                  std::shared_ptr<std::atomic<bool>> stop) {
  if (maxDepth <= 0) maxDepth = pimpl->cfg.maxDepth;
  return std::nullopt;
  pimpl->search->search_root_parallel(pos, maxDepth, stop, pimpl->cfg.threads);

  return pimpl->search->getStats().bestMove;
}

const SearchStats& Engine::getLastSearchStats() const {
  return pimpl->search->getStats();
}
const EngineConfig& Engine::getConfig() const {
  return pimpl->cfg;
}

}  // namespace lilia::engine
