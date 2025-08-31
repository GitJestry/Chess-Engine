#pragma once

#include <string>

#include "../chess_types.hpp"
#include "../constants.hpp"

namespace lilia::app {

class App {
 public:
  App() = default;
  int run();

 private:
  bool m_white_is_bot = false;
  bool m_black_is_bot = true;
  std::string m_start_fen = core::START_FEN;
  int m_thinkTimeMs = 10000;  // Bot think time in milliseconds
  int m_searchDepth = 10;     // Search depth for bot
};

}  // namespace lilia::app

