#include <iostream>
#include <string>

#include "lilia/controller/bot_player.hpp"
#include "lilia/model/chess_game.hpp"
#include "uci_helper.hpp"

namespace lilia {

class UCI {
 public:
  UCI() = default;
  int run();

 private:
  void showOptions();
  void setOption();
  std::string m_name = "LiliaEngine";
  std::string m_version = "1.0";
  controller::BotPlayer lilia;
  model::ChessGame game;
};
}  // namespace lilia
