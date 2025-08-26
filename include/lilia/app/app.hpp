#pragma once

#include <string>

#include "../chess_types.hpp"

namespace lilia::app {

class App {
 public:
  App() = default;
  // Run the application. Returns the program exit code.
  int run();

 private:
  // Read start options from terminal; empty input -> defaults
  void promptStartOptions();

  // input helper: parse integer with defaults and bounds
  static int parseIntInRange(const std::string& s, int defaultVal, int minVal,
                             int maxVal);

  // helpers
  static std::string trim(const std::string& s);
  static std::string toLower(const std::string& s);
  static bool parseYesNoDefaultTrue(const std::string& s);

  // parsed options
  core::Color m_playerColor = core::Color::White;
  bool m_vsBot = true;
  std::string m_startFen;
  int m_thinkTimeMs = 1000;  // Bot think time in milliseconds
  int m_searchDepth = 5;     // Search depth for bot
};

}  // namespace lilia::app
