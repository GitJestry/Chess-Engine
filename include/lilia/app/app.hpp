#pragma once

#include <string>

#include "../chess_types.hpp"

namespace lilia::app {

class App {
 public:
  App() = default;
  int run();

 private:
  void promptStartOptions();

  // input helper: parse integer with defaults and bounds
  static int parseIntInRange(const std::string& s, int defaultVal, int minVal, int maxVal);

  // helpers

  static std::string trim(const std::string& s);
  static std::string toLower(const std::string& s);
  static bool parseYesNo(const std::string& s, bool defaultVal);

  // parsed options
  bool m_white_is_bot = false;
  bool m_black_is_bot = true;
  std::string m_start_fen;
  int m_thinkTimeMs = 10000;  // Bot think time in milliseconds
  int m_searchDepth = 10;     // Search depth for bot
};

}  // namespace lilia::app
