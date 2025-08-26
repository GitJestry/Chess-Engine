#include "lilia/controller/bot_player.hpp"

#include <atomic>
#include <future>
#include <iostream>

#include "lilia/engine/bot_engine.hpp"
#include "lilia/model/chess_game.hpp"
#include "lilia/model/move_generator.hpp"  // falls MoveGenerator an anderem Ort, anpassen
#include "lilia/uci/uci_helper.hpp"

namespace lilia::controller {

std::future<model::Move> BotPlayer::requestMove(model::ChessGame& gameState,
                                                std::atomic<bool>& cancelToken) {
  int requestedDepth = m_depth;
  int thinkMs = m_thinkMillis;

  return std::async(std::launch::async,
                    [this, &gameState, &cancelToken, requestedDepth, thinkMs]() -> model::Move {
                      
                      
                      lilia::engine::BotEngine engine;

                      
                      lilia::engine::SearchResult res =
                          engine.findBestMove(gameState, requestedDepth, thinkMs, &cancelToken);

                      
                      if (cancelToken.load()) {
                        return model::Move{};  
                      }

                      
                      
                      if (!res.bestMove.has_value()) {
                        model::MoveGenerator mg;
                        thread_local std::vector<model::Move> moveBuf;
                        auto pos = gameState.getPositionRefForBot();
                        mg.generatePseudoLegalMoves(pos.board(), pos.state(), moveBuf);
                        for (auto& m : moveBuf) {
                          if (pos.doMove(m)) {
                            pos.undoMove();
                            std::cout << "[BotPlayer] fallback move chosen: " << move_to_uci(m)
                                      << "\n";
                            return m;
                          }
                        }

                        std::cout << "[BotPlayer] returning invalid move (no legal moves)\n";
                        return model::Move{};
                      }

                      return res.bestMove.value_or(model::Move{});
                    });
}

}  
