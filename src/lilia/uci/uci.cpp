// src/lilia/uci/uci.cpp
#include "lilia/uci/uci.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "lilia/engine/bot_engine.hpp"
#include "lilia/model/chess_game.hpp"
#include "lilia/uci/uci_helper.hpp"  // move_to_uci

namespace lilia {

// split by whitespace
static std::vector<std::string> split_ws(const std::string& s) {
  std::istringstream iss(s);
  std::vector<std::string> out;
  std::string tok;
  while (iss >> tok) out.push_back(tok);
  return out;
}

static std::string extract_fen_after(const std::string& line) {
  auto pos = line.find("fen");
  if (pos == std::string::npos) return "";
  pos += 3;
  while (pos < line.size() && isspace((unsigned char)line[pos])) ++pos;
  auto moves_pos = line.find(" moves ", pos);
  if (moves_pos == std::string::npos) return line.substr(pos);
  return line.substr(pos, moves_pos - pos);
}

void UCI::showOptions() {
  std::cout << "option name Hash type spin default 64 min 1 max 131072\n";
  std::cout << "option name Threads type spin default 1 min 1 max 64\n";
  std::cout << "option name Ponder type check default false\n";
}

void UCI::setOption(const std::string& line) {
  auto tokens = split_ws(line);
  std::string name;
  std::string value;
  for (size_t i = 1; i + 1 < tokens.size(); ++i) {
    if (tokens[i] == "name") {
      size_t j = i + 1;
      std::ostringstream n;
      while (j < tokens.size() && tokens[j] != "value") {
        if (n.tellp() > 0) n << ' ';
        n << tokens[j++];
      }
      name = n.str();
    }
    if (tokens[i] == "value") {
      size_t j = i + 1;
      std::ostringstream v;
      while (j < tokens.size()) {
        if (v.tellp() > 0) v << ' ';
        v << tokens[j++];
      }
      value = v.str();
    }
  }
  if (name.empty()) return;

  static std::map<std::string, std::string> options;
  options[name] = value;
  // optional: map options to engine config if you expose setters later
}

// UCI run: verwendet BotEngine direkt (kein controller)
int UCI::run() {
  std::string line;

  // search state
  std::mutex stateMutex;
  std::future<lilia::model::Move> searchFuture;
  std::thread printerThread;
  std::atomic<bool> cancelToken(false);
  bool searchRunning = false;

  // local engine instance not in controller
  // We will create an engine per-search (stateless) or reuse one as needed.
  // Here we create per-search to avoid thread-safety concerns.
  while (std::getline(std::cin, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) continue;

    auto tokens = split_ws(line);
    if (tokens.empty()) continue;
    const std::string& cmd = tokens[0];

    if (cmd == "uci") {
      std::cout << "id name " << m_name << "\n";
      std::cout << "id version " << m_version << "\n";
      showOptions();
      std::cout << "uciok\n";
      continue;
    }

    if (cmd == "isready") {
      std::cout << "readyok\n";
      continue;
    }

    if (cmd == "setoption") {
      setOption(line);
      continue;
    }

    if (cmd == "ucinewgame") {
      // optional: reset engine state if you keep persistent engine
      continue;
    }

    if (cmd == "position") {
      if (line.find("startpos") != std::string::npos) {
        m_game.setPosition(core::START_FEN);
      } else if (line.find("fen") != std::string::npos) {
        std::string fen = extract_fen_after(line);
        if (!fen.empty()) {
          m_game.setPosition(fen);
        }
      }
      auto posMoves = line.find("moves");
      if (posMoves != std::string::npos) {
        std::string movesStr = line.substr(posMoves + 6);
        std::istringstream iss(movesStr);
        std::string moveUci;
        while (iss >> moveUci) {
          try {
            m_game.doMoveUCI(moveUci);
          } catch (...) {
            std::cerr << "[UCI] warning: applyMoveUCI failed for " << moveUci << "\n";
          }
        }
      }
      continue;
    }

    if (cmd == "go") {
      int depth = -1;
      int movetime = -1;
      bool infinite = false;
      for (size_t i = 1; i < tokens.size(); ++i) {
        if (tokens[i] == "depth" && i + 1 < tokens.size()) {
          depth = std::stoi(tokens[++i]);
        } else if (tokens[i] == "movetime" && i + 1 < tokens.size()) {
          movetime = std::stoi(tokens[++i]);
        } else if (tokens[i] == "infinite") {
          infinite = true;
        }
      }

      // stop running search if any
      {
        std::lock_guard<std::mutex> lk(stateMutex);
        if (searchRunning) {
          cancelToken.store(true);
          if (printerThread.joinable()) printerThread.join();
          searchRunning = false;
          cancelToken.store(false);
        }
      }

      // determine time to think (milliseconds). If movetime given use it, else if infinite use 0
      // (no timer), else 0
      int thinkMillis = (movetime > 0 ? movetime : 0);

      // Start asynchronous search using engine::BotEngine synchronously inside async
      cancelToken.store(false);
      {
        std::lock_guard<std::mutex> lk(stateMutex);
        // use std::async to run the engine call in background
        searchFuture = std::async(
            std::launch::async, [this, depth, thinkMillis, &cancelToken]() -> model::Move {
              lilia::engine::BotEngine engine;
              auto res = engine.findBestMove(const_cast<model::ChessGame&>(m_game),
                                             (depth > 0 ? depth : /*some default*/ 0), thinkMillis,
                                             &cancelToken);
              if (res.bestMove.has_value()) return res.bestMove.value();
              return model::Move{};  // invalid move if none
            });

        searchRunning = true;

        // printer thread: wait for future and print bestmove
        printerThread = std::thread([&searchFuture, &stateMutex, &searchRunning, &cancelToken]() {
          model::Move best;
          try {
            best = searchFuture.get();
          } catch (...) {
            best = model::Move{};
          }

          if (best.from >= 0 && best.to >= 0) {
            std::cout << "bestmove " << move_to_uci(best) << "\n";
          } else {
            // UCI requires a move; if none found, send bestmove with invalid (but better send
            // resign?) We'll send bestmove 0000 (some GUIs might treat it as invalid) —
            // alternative: pick a legal move before returning.
            std::cout << "bestmove 0000\n";
          }

          {
            std::lock_guard<std::mutex> lk2(stateMutex);
            searchRunning = false;
            cancelToken.store(false);
          }
        });
      }

      continue;
    }  // end go

    if (cmd == "stop") {
      cancelToken.store(true);
      continue;
    }

    if (cmd == "quit") {
      cancelToken.store(true);
      {
        std::lock_guard<std::mutex> lk(stateMutex);
        if (searchRunning && printerThread.joinable()) {
          printerThread.join();
        }
      }
      break;
    }

    // unknown command -> ignore
  }  // while getline

  // cleanup
  {
    std::lock_guard<std::mutex> lk(stateMutex);
    if (searchRunning && printerThread.joinable()) printerThread.join();
  }

  return 0;
}

}  // namespace lilia
