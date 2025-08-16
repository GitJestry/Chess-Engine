#pragma once
#include <string>

namespace lilia {
namespace core {
constexpr unsigned int BOARD_SIZE = 8;
constexpr unsigned int WINDOW_PX_SIZE = 800;
constexpr unsigned int SQUARE_PX_SIZE = WINDOW_PX_SIZE / BOARD_SIZE;
constexpr unsigned int ATTACK_DOT_PX_SIZE =
    static_cast<unsigned int>(static_cast<float>(SQUARE_PX_SIZE) * 0.25f + 0.5f);
constexpr unsigned int HOVER_PX_SIZE = SQUARE_PX_SIZE;
constexpr float ANIM_SNAP_SPEED = .1f;
constexpr float ANIM_MOVE_SPEED = .05f;

const std::string START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
const std::string STR_TEXTURE_WHITE = "white";
const std::string STR_TEXTURE_BLACK = "black";
const std::string STR_TEXTURE_TRANSPARENT = "transparent";
const std::string STR_TEXTURE_SELECTHLIGHT = "selHlight";
const std::string STR_TEXTURE_ATTACKHLIGHT = "attHlight";
const std::string STR_TEXTURE_HOVERHLIGHT = "hovHlight";

}  // namespace core

}  // namespace lilia
