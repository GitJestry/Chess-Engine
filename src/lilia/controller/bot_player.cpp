#include "lilia/controller/bot_player.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <random>
#include <thread>

namespace lilia::controller {

std::future<model::Move> BotPlayer::requestMove(model::ChessGame& gameState,
                                                std::atomic<bool>& cancelToken) {
  // Wir geben einen std::future zurück, die Suche läuft asynchron.
  // Member m_thinkMillis (int) soll die erlaubte Denkzeit in Millisekunden enthalten.
  return std::async(std::launch::async, [this, &gameState, &cancelToken]() -> model::Move {
    // Kopie der Position, Engine arbeitet auf der Kopie
    auto pos = gameState.getPositionRefForBot();

    // Kooperatives Stop-Flag, wird vom Controller/Timer gesetzt
    std::atomic<bool> stopFlag(false);

    // Timer-Thread: setzt stopFlag nach m_thinkMillis oder wenn cancelToken gesetzt wird.
    std::thread timer([&]() {
      using namespace std::chrono;
      auto start = steady_clock::now();
      while (!stopFlag.load() && !cancelToken.load()) {
        auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start).count();
        if (elapsed >= m_thinkMillis) {
          stopFlag.store(true);
          break;
        }
        // Sleep kurz, damit wir nicht busy-loopen (Granularität ~5ms)
        std::this_thread::sleep_for(milliseconds(5));
      }
    });

    // Suche starten (synchron in diesem async-Thread) — Engine muss kooperatives Stop unterstützen
    std::optional<model::Move> result;
    try {
      // übergebe Adresse des stopFlags an die Engine
      result = m_engine.find_best_move(pos, /*maxDepth=*/12, &stopFlag);
    } catch (...) {
      // defensive: bei Ausnahme -> sicherstellen timer beendet wird
      stopFlag.store(true);
    }

    // Timer stoppen und joinen
    if (timer.joinable()) timer.join();

    // Wenn externer cancelToken gesetzt -> liefern wir einen invaliden Move als Abbruchsignal
    if (cancelToken.load()) {
      return model::Move{};  // invalider Move als Abbruch-Indikator
    }

    // Falls Engine kein Move gefunden hat, fallback: erster legaler Move (oder invalider Move)
    if (!result.has_value()) {
      // Versuch: suche einfachen legalen Move als Fallback
      model::MoveGenerator mg;
      auto moves = mg.generatePseudoLegalMoves(pos.board(), pos.state());
      for (auto& m : moves) {
        if (pos.doMove(m)) {
          pos.undoMove();
          return m;
        }
      }
      return model::Move{};  // nichts gefunden
    }

    return result.value();
  });
}

}  // namespace lilia::controller
