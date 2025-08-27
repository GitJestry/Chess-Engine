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
  static bool parseYesNoDefaultTrue(const std::string& s);

  // parsed options
  core::Color m_player_color = core::Color::White;
  bool m_vs_bot = true;
  std::string m_start_fen;
  int m_thinkTimeMs = 10000;  // Bot think time in milliseconds
  int m_searchDepth = 10;     // Search depth for bot
};

}  // namespace lilia::app
