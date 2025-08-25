#include "lilia/engine/engine.hpp"

#include "lilia/engine/search.hpp"
#include "lilia/model/core/magic.hpp"

namespace lilia::engine {

struct Engine::Impl {
  EngineConfig cfg;
  model::TT4 tt;
  Evaluator eval;
  Search* search = nullptr;
  Impl(const EngineConfig& c) : cfg(c), tt(c.ttSizeMb) { search = new Search(tt, eval, cfg); }
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
  pimpl->search->search_root(pos, maxDepth, stop);
  return getLastSearchStats().bestMove;
}

SearchStats Engine::getLastSearchStats() const {
  return pimpl->search->getStatsCopy();
}

}  // namespace lilia
