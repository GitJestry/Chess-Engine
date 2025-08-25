#include "lilia/engine/bot_engine.hpp"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

#include "lilia/uci/uci_helper.hpp"  // für move_to_uci falls gewünscht beim Logging

namespace lilia::engine {

BotEngine::BotEngine() : m_engine() {}
BotEngine::~BotEngine() = default;

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

SearchResult BotEngine::findBestMove(model::ChessGame& gameState, int maxDepth, int thinkMillis,
                                     std::atomic<bool>* externalCancel) {
  SearchResult res;
  auto pos = gameState.getPositionRefForBot();

  std::atomic<bool> stopFlag(false);

  std::mutex m;
  std::condition_variable cv;
  bool timerStop = false;

  std::thread timer([&]() {
    if (thinkMillis <= 0) return;  // no timer requested
    std::unique_lock<std::mutex> lk(m);
    bool pred = cv.wait_for(lk, std::chrono::milliseconds(thinkMillis), [&] {
      return timerStop || (externalCancel && externalCancel->load());
    });
    if (!pred) {
      stopFlag.store(true);
    } else {
      if (externalCancel && externalCancel->load()) {
        stopFlag.store(true);
      }
    }
  });

  using steady_clock = std::chrono::steady_clock;
  auto t0 = steady_clock::now();
  bool engineThrew = false;
  std::string engineErr;

  try {
    auto mv = m_engine.find_best_move(pos, maxDepth, &stopFlag);
    res.bestMove = mv;
  } catch (const std::exception& e) {
    engineThrew = true;
    engineErr = std::string("exception: ") + e.what();
    std::cerr << "[BotEngine] engine threw exception: " << e.what() << "\n";
    res.bestMove = std::nullopt;
  } catch (...) {
    engineThrew = true;
    engineErr = "unknown exception";
    std::cerr << "[BotEngine] engine threw unknown exception\n";
    res.bestMove = std::nullopt;
  }

  auto t1 = steady_clock::now();
  long long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

  // signal timer to stop and join
  {
    std::lock_guard<std::mutex> lk(m);
    timerStop = true;
  }
  cv.notify_one();
  if (timer.joinable()) timer.join();

  // reason (for logging)
  std::string reason;
  if (externalCancel && externalCancel->load()) {
    reason = "external-cancel";
  } else if (engineThrew) {
    reason = std::string("exception: ") + engineErr;
  } else if (stopFlag.load() && thinkMillis > 0 && elapsedMs >= thinkMillis) {
    reason = "timeout";
  } else {
    reason = "normal";
  }

  // collect stats from engine
  res.stats = m_engine.getLastSearchStats();
  res.topMoves = res.stats.topMoves;

  // Logging (similar style as before)
  std::cout << "\n";
  std::cout << "[BotEngine] Search finished: depth=" << maxDepth << " time=" << elapsedMs
            << "ms threads=" << m_engine.getConfig().threads << " reason=" << reason << "\n";

  std::cout << "[BotEngine] info nodes=" << res.stats.nodes
            << " nps=" << static_cast<long long>(res.stats.nps) << " time=" << res.stats.elapsedMs
            << " bestScore=" << res.stats.bestScore;
  if (res.stats.bestMove.has_value()) {
    std::cout << " bestMove=" << move_to_uci(res.stats.bestMove.value());
  }
  std::cout << "\n";

  if (!res.stats.bestPV.empty()) {
    std::cout << "[BotEngine] pv ";
    bool first = true;
    for (auto& mv : res.stats.bestPV) {
      if (!first) std::cout << " ";
      first = false;
      std::cout << move_to_uci(mv);
    }
    std::cout << "\n";
  }

  if (!res.topMoves.empty()) {
    std::cout << "[BotEngine] topMoves " << format_top_moves(res.topMoves) << "\n";
  }

  return res;
}

SearchStats BotEngine::getLastSearchStats() const {
  return m_engine.getLastSearchStats();
}

}  // namespace lilia::engine
