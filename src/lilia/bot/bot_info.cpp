#include "lilia/bot/bot_info.hpp"

namespace lilia {

PlayerInfo getBotInfo(BotType type) {
  switch (type) {
    case BotType::Lilia:
    default:
      return {"Lilia", 2000, "assets/textures/5.png"};
  }
}

} // namespace lilia

