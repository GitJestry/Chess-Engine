#include <cassert>
#include <memory>
#include <atomic>

#include "lilia/engine/bot_engine.hpp"
#include "lilia/engine/search.hpp"
#include "lilia/engine/eval.hpp"
#include "lilia/model/chess_game.hpp"
#include "lilia/model/tt5.hpp"

using namespace lilia;

static core::Square sq(char file, int rank) {
  int f = file - 'a';
  int r = rank - 1;
  return static_cast<core::Square>(r * 8 + f);
}

int main() {
  engine::EngineConfig cfg;
  engine::BotEngine bot(cfg);

  // Quiet piece move giving check
  {
    model::ChessGame game;
    game.setPosition("4k3/8/8/8/4N3/8/8/4K3 w - - 0 1");
    auto res = bot.findBestMove(game, 2, 10);
    assert(res.bestMove);
    model::Move expected(sq('e',4), sq('f',6));
    assert(*res.bestMove == expected);
  }

  // Quiet piece move threatening a rook
  {
    model::ChessGame game;
    game.setPosition("4r2k/8/6B1/8/8/8/8/4K3 w - - 0 1");
    auto res = bot.findBestMove(game, 2, 10);
    assert(res.bestMove);
    model::Move expected(sq('g',6), sq('f',7));
    assert(*res.bestMove == expected);
  }

  // Best move should match the first entry in topMoves even when TT suggests a different move
  {
    model::ChessGame game;
    game.setPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    auto& pos = game.getPositionRefForBot();

    model::TT5 tt;
    engine::Evaluator eval;
    auto evalPtr = std::shared_ptr<const engine::Evaluator>(&eval, [](const engine::Evaluator*){});
    engine::Search search(tt, evalPtr, cfg);

    model::Move wrong(sq('a', 2), sq('a', 3));
    tt.store(pos.hash(), 0, 1, model::Bound::Exact, wrong);

    auto stop = std::make_shared<std::atomic<bool>>(false);
    search.search_root_single(pos, 2, stop, 0);
    const auto& stats = search.getStats();
    assert(!stats.topMoves.empty());
    assert(stats.bestMove == stats.topMoves[0].first);
  }

  // topMoves should report distinct scores for different moves
  {
    model::ChessGame game;
    game.setPosition("4k3/8/8/7Q/8/8/8/4K3 w - - 0 1");
    auto& pos = game.getPositionRefForBot();

    model::TT5 tt;
    engine::Evaluator eval;
    auto evalPtr = std::shared_ptr<const engine::Evaluator>(&eval, [](const engine::Evaluator*){});
    engine::Search search(tt, evalPtr, cfg);

    auto stop = std::make_shared<std::atomic<bool>>(false);
    search.search_root_single(pos, 3, stop, 0);
    const auto& stats = search.getStats();
    assert(stats.topMoves.size() >= 2);
    assert(stats.topMoves[0].second != stats.topMoves[1].second);
  }

  return 0;
}
