#pragma once
#include <string>

#include "lilia/model/chess_game.hpp"

namespace lilia {

class UCI {
 public:
  UCI() = default;
  int run();

 private:
  void showOptions();
  void setOption(const std::string& line);

  std::string m_name = "LiliaEngine";
  std::string m_version = "1.0";

  model::ChessGame m_game;
};

}  
