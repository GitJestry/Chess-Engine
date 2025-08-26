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

  // helpers
  static std::string trim(const std::string& s);
  static std::string toLower(const std::string& s);
  static bool parseYesNoDefaultTrue(const std::string& s);

  // parsed options
  core::Color m_player_color = core::Color::White;
  bool m_vs_bot = true;
  std::string m_start_fen;
};

}  // namespace lilia::app
