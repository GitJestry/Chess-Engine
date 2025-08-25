#include "lilia/controller/bot_player.hpp"

#include <atomic>
#include <future>
#include <iostream>

#include "lilia/engine/bot_engine.hpp"
#include "lilia/model/move_generator.hpp"  // falls MoveGenerator an anderem Ort, anpassen
#include "lilia/uci/uci_helper.hpp"

namespace lilia::controller {

std::future<model::Move> BotPlayer::requestMove(model::ChessGame& gameState,
                                                std::atomic<bool>& cancelToken) {
  int requestedDepth = m_depth;
  int thinkMs = m_thinkMillis;

  return std::async(std::launch::async,
                    [this, &gameState, &cancelToken, requestedDepth, thinkMs]() -> model::Move {
                      // Erzeuge lokalen BotEngine (Thread-sicher, keine gemeinsame Engine-Instanz
                      // nötig)
                      lilia::engine::BotEngine engine;

                      // Führe Suche aus (synchron innerhalb des async-Tasks)
                      lilia::engine::SearchResult res =
                          engine.findBestMove(gameState, requestedDepth, thinkMs, &cancelToken);

                      // Wenn externer cancelToken gesetzt -> invalider Move als Abbruchsignal
                      if (cancelToken.load()) {
                        return model::Move{};  // invalider Move als Abbruch-Indikator
                      }

                      // Falls Engine kein Move gefunden hat, fallback: erster legaler Move (oder
                      // invalider Move)
                      if (!res.bestMove.has_value()) {
                        model::MoveGenerator mg;
                        auto pos = gameState.getPositionRefForBot();
                        auto moves = mg.generatePseudoLegalMoves(pos.board(), pos.state());
                        for (auto& m : moves) {
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

}  // namespace lilia::controller
