#include "lilia/controller/bot_player.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>

// include SearchStats definition
#include "lilia/engine/search.hpp"
#include "lilia/uci/uci_helper.hpp"

namespace lilia::controller {

// helper: print top-K moves vector
static inline std::string format_top_moves(const std::vector<std::pair<model::Move, int>>& top) {
  std::string out;
  bool first = true;
  for (auto& p : top) {
    if (!first) out += ", ";
    first = false;
    out += move_to_uci(p.first) + " (" + std::to_string(p.second) + ")";
  }
  if (out.empty()) out = "<none>";
  return out;
}

std::future<model::Move> BotPlayer::requestMove(model::ChessGame& gameState,
                                                std::atomic<bool>& cancelToken) {
  int requestedDepth = m_depth;

  return std::async(
      std::launch::async, [this, &gameState, &cancelToken, requestedDepth]() -> model::Move {
        auto pos = gameState.getPositionRefForBot();
        std::atomic<bool> stopFlag(false);

        // Synchronisationshilfen für den Timer (keine busy-wait)
        std::mutex m;
        std::condition_variable cv;
        bool timerStop =
            false;  // wird true gesetzt, wenn Engine fertig ist und Timer abbrechen soll
        std::thread timer([&]() {
          std::unique_lock<std::mutex> lk(m);
          bool pred = cv.wait_for(lk, std::chrono::milliseconds(m_thinkMillis),
                                  [&] { return timerStop || cancelToken.load(); });
          if (!pred) {
            stopFlag.store(true);
          } else {
            if (cancelToken.load()) {
              stopFlag.store(true);
            }
          }
        });

        std::optional<model::Move> result;
        using steady_clock = std::chrono::steady_clock;
        auto t0 = steady_clock::now();
        bool engineThrew = false;
        std::string engineErr;

        // Call engine inside try/catch so we always can cleanup timer and join the thread.
        try {
          result = m_engine.find_best_move(pos, /*maxDepth=*/requestedDepth, &stopFlag);
        } catch (const std::bad_optional_access& e) {
          engineThrew = true;
          engineErr = std::string("bad_optional_access: ") + e.what();
          std::cerr << "[BotPlayer] engine threw bad_optional_access: " << e.what() << "\n";
          result = std::nullopt;
        } catch (const std::exception& e) {
          engineThrew = true;
          engineErr = std::string("exception: ") + e.what();
          std::cerr << "[BotPlayer] engine threw exception: " << e.what() << "\n";
          result = std::nullopt;
        } catch (...) {
          engineThrew = true;
          engineErr = "unknown exception";
          std::cerr << "[BotPlayer] engine threw unknown exception\n";
          result = std::nullopt;
        }

        auto t1 = steady_clock::now();
        long long elapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

        // Engine ist fertig (oder hat geworfen): signalisiere dem Timer, dass er abbrechen soll
        {
          std::lock_guard<std::mutex> lk(m);
          timerStop = true;
        }
        cv.notify_one();

        // Timer stoppen und joinen (immer versuchen, auch nach Ausnahme)
        if (timer.joinable()) timer.join();

        // Rich logging: Laufzeit + Grund + SearchStats (falls Engine/Search bereitstellt)
        std::string reason;
        if (cancelToken.load()) {
          reason = "external-cancel";
        } else if (engineThrew) {
          reason = std::string("exception: ") + engineErr;
        } else if (stopFlag.load() && elapsedMs >= m_thinkMillis) {
          reason = "timeout";
        } else {
          reason = "normal";
        }

        engine::SearchStats s = m_engine.getLastSearchStats();

        std::cout << "\n";
        std::cout << "[BotPlayer] Search finished: depth=" << requestedDepth
                  << " time=" << elapsedMs << "ms reason=" << reason << "\n";

        // structured info similar to UCI 'info' line
        std::cout << "[BotPlayer] info nodes=" << s.nodes
                  << " nps=" << static_cast<long long>(s.nps) << " time=" << s.elapsedMs
                  << " bestScore=" << s.bestScore;
        if (s.bestMove.has_value()) {
          std::cout << " bestMove=" << move_to_uci(s.bestMove.value());
        }
        std::cout << "\n";

        // print PV
        if (!s.bestPV.empty()) {
          std::cout << "[BotPlayer] pv ";
          bool first = true;
          for (auto& mv : s.bestPV) {
            if (!first) std::cout << " ";
            first = false;
            std::cout << move_to_uci(mv);
          }
          std::cout << "\n";
        }

        // print Top-K
        if (!s.topMoves.empty()) {
          std::cout << "[BotPlayer] topMoves " << format_top_moves(s.topMoves) << "\n";
        }

        // Wenn externer cancelToken gesetzt -> liefern wir einen invaliden Move als Abbruchsignal
        if (cancelToken.load()) {
          return model::Move{};  // invalider Move als Abbruch-Indikator
        }

        // Falls Engine kein Move gefunden hat, fallback: erster legaler Move (oder invalider
        // Move)
        if (!result.has_value()) {
          // Versuch: suche einfachen legalen Move als Fallback
          model::MoveGenerator mg;
          auto moves = mg.generatePseudoLegalMoves(pos.board(), pos.state());
          for (auto& m : moves) {
            if (pos.doMove(m)) {
              pos.undoMove();
              std::cout << "[BotPlayer] fallback move chosen: " << move_to_uci(m) << "\n";
              return m;
            }
          }

          std::cout << "[BotPlayer] returning invalid move (no legal moves)\n";
          return model::Move{};  // nichts gefunden
        }

        // final: sicher zurückgeben (falls result leer ist, gib invaliden Move zurück)
        return result.value_or(model::Move{});
      });
}

}  // namespace lilia::controller
