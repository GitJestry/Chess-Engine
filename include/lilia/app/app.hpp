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

  static std::string trim(const std::string& s);
  static std::string toLower(const std::string& s);
  static bool parseYesNoDefaultTrue(const std::string& s);

  core::Color m_playerColor = core::Color::White;
  bool m_vsBot = true;
  std::string m_startFen;
};

}  
