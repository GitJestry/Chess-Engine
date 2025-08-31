#pragma once

#include "lilia/player_info.hpp"

namespace lilia {

enum class BotType { Lilia };

PlayerInfo getBotInfo(BotType type);

} // namespace lilia

