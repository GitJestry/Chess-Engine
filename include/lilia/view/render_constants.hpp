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
const std::string STR_TEXTURE_SELECTHLIGHT = "selectHighlight";
const std::string STR_TEXTURE_ATTACKHLIGHT = "attackHighlight";
const std::string STR_TEXTURE_HOVERHLIGHT = "hoverHighlight";

const std::string ASSET_PIECES_FILE_PATH = "assets/textures";

const std::string ASSET_SFX_FILE_PATH = "assets/audio/sfx";
const std::string SFX_PLAYER_MOVE_NAME = "player_move";
const std::string SFX_ENEMY_MOVE_NAME = "enemy_move";
const std::string SFX_CAPTURE_NAME = "capture";
const std::string SFX_CASTLE_NAME = "castle";
const std::string SFX_CHECK_NAME = "check";
const std::string SFX_GAME_BEGINS_NAME = "game_begins";
const std::string SFX_GAME_ENDS_NAME = "game_ends";

}  // namespace core

}  // namespace lilia
