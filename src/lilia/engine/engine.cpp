#include "lilia/engine/engine.hpp"

#include <algorithm>

#include "lilia/engine/search.hpp"
#include "lilia/model/core/magic.hpp"

namespace lilia::engine {

struct Engine::Impl {
  EngineConfig cfg;
  model::TT4 tt;
  Evaluator eval;
  Search* search = nullptr;
  Impl(const EngineConfig& c) : cfg(c), tt(c.ttSizeMb) {
    unsigned int hw = std::thread::hardware_concurrency();  // 0 if unknown
    int logical = (hw > 0 ? (int)hw : 1);
    cfg.threads = std::max(1, logical - 1);
    search = new Search(tt, eval, cfg);
  }
  ~Impl() { delete search; }
};

Engine::Engine(const EngineConfig& cfg) : pimpl(new Impl(cfg)) {}
Engine::~Engine() {
  delete pimpl;
}

std::optional<model::Move> Engine::find_best_move(model::Position& pos, int maxDepth,
                                                  std::atomic<bool>* stop) {
  if (maxDepth <= 0) maxDepth = pimpl->cfg.maxDepth;
  pimpl->tt.new_generation();

  pimpl->search->search_root_parallel(pos, maxDepth, stop, pimpl->cfg.threads);

  return pimpl->search->getStatsCopy().bestMove;
}

SearchStats Engine::getLastSearchStats() const {
  return pimpl->search->getStatsCopy();
}
EngineConfig Engine::getConfig() const {
  return pimpl->cfg;
}

}  // namespace lilia::engine
