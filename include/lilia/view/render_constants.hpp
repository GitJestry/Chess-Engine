#pragma once
#include "../constants.hpp"

namespace lilia::view::constant {
constexpr unsigned int BOARD_SIZE = 8;

// Board and window dimensions are now decoupled so the board can keep a
// fixed size while the window grows to host additional UI elements such as an
// evaluation bar, clocks or player names.
constexpr unsigned int BOARD_PX_SIZE = 800;
constexpr unsigned int SIDEBAR_PX_SIZE = 200;  // space for evaluation bar & future UI
constexpr unsigned int EVAL_BAR_PX_WIDTH = 80;
constexpr unsigned int WINDOW_PX_WIDTH = BOARD_PX_SIZE + SIDEBAR_PX_SIZE;
constexpr unsigned int WINDOW_PX_HEIGHT = BOARD_PX_SIZE;
constexpr unsigned int BOARD_OFFSET_X = SIDEBAR_PX_SIZE;
constexpr unsigned int SQUARE_PX_SIZE = BOARD_PX_SIZE / BOARD_SIZE;
constexpr unsigned int ATTACK_DOT_PX_SIZE =
    static_cast<unsigned int>(static_cast<float>(SQUARE_PX_SIZE) * 0.45f + 0.5f);
constexpr unsigned int CAPTURE_CIRCLE_PX_SIZE =
    static_cast<unsigned int>(static_cast<float>(SQUARE_PX_SIZE) * 1.02f + 0.5f);

constexpr unsigned int HOVER_PX_SIZE = SQUARE_PX_SIZE;

constexpr float ANIM_SNAP_SPEED = .005f;
constexpr float ANIM_MOVE_SPEED = .05f;

const std::string STR_TEXTURE_WHITE = "white";
const std::string STR_TEXTURE_BLACK = "black";

const std::string STR_TEXTURE_PROMOTION = "promotion";
const std::string STR_TEXTURE_PROMOTION_SHADOW = "promotionShadow";

const std::string STR_TEXTURE_TRANSPARENT = "transparent";
const std::string STR_TEXTURE_SELECTHLIGHT = "selectHighlight";
const std::string STR_TEXTURE_ATTACKHLIGHT = "attackHighlight";
const std::string STR_TEXTURE_CAPTUREHLIGHT = "captureHighlight";
const std::string STR_TEXTURE_HOVERHLIGHT = "hoverHighlight";
const std::string STR_TEXTURE_WARNINGHLIGHT = "warningHighlight";

const std::string STR_FILE_PATH_HAND_OPEN = "assets/textures/cursor_hand_open.png";
const std::string STR_FILE_PATH_HAND_CLOSED = "assets/textures/cursor_hand_closed.png";

const std::string ASSET_PIECES_FILE_PATH = "assets/textures";
constexpr float ASSET_PIECE_SCALE = 1.6f;

const std::string ASSET_SFX_FILE_PATH = "assets/audio/sfx";
const std::string SFX_PLAYER_MOVE_NAME = "player_move";
const std::string SFX_ENEMY_MOVE_NAME = "enemy_move";
const std::string SFX_WARNING_NAME = "warning";
const std::string SFX_CAPTURE_NAME = "capture";
const std::string SFX_CASTLE_NAME = "castle";
const std::string SFX_CHECK_NAME = "check";
const std::string SFX_PROMOTION_NAME = "promotion";
const std::string SFX_GAME_BEGINS_NAME = "game_begins";
const std::string SFX_GAME_ENDS_NAME = "game_ends";

}  // namespace lilia::view::constant
