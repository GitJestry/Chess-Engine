#include <cassert>
#include <memory>
#include <atomic>

#include "lilia/engine/bot_engine.hpp"
#include "lilia/model/chess_game.hpp"

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

  return 0;
}
