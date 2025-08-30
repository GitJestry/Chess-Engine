#pragma once
#include "../constants.hpp"
#include <algorithm>

namespace lilia::view::constant {
constexpr unsigned int BOARD_SIZE = 8;
// Distance between the window border and the board on each side
inline constexpr unsigned int WINDOW_MARGIN = 100;
inline unsigned int WINDOW_WIDTH = 1000;
inline unsigned int WINDOW_HEIGHT = 1000;
inline unsigned int WINDOW_PX_SIZE = 800;  // Board edge length
inline unsigned int BOARD_OFFSET_X = WINDOW_MARGIN;
inline unsigned int BOARD_OFFSET_Y = WINDOW_MARGIN;
inline unsigned int SQUARE_PX_SIZE = WINDOW_PX_SIZE / BOARD_SIZE;
inline unsigned int ATTACK_DOT_PX_SIZE =
    static_cast<unsigned int>(static_cast<float>(SQUARE_PX_SIZE) * 0.45f + 0.5f);
inline unsigned int CAPTURE_CIRCLE_PX_SIZE =
    static_cast<unsigned int>(static_cast<float>(SQUARE_PX_SIZE) * 1.02f + 0.5f);

inline unsigned int EVAL_BAR_HEIGHT = WINDOW_PX_SIZE;
inline unsigned int EVAL_BAR_WIDTH = BOARD_OFFSET_X;

inline unsigned int HOVER_PX_SIZE = SQUARE_PX_SIZE;

inline float ANIM_SNAP_SPEED = .005f;
inline float ANIM_MOVE_SPEED = .05f;

const std::string STR_TEXTURE_WHITE = "white";
const std::string STR_TEXTURE_BLACK = "black";
const std::string STR_TEXTURE_EVAL_WHITE = "evalwhite";
const std::string STR_TEXTURE_EVAL_BLACK = "evalblack";

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
inline float ASSET_PIECE_SCALE = 1.6f;  // base scale for 100px squares

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

inline void updateWindowDimensions(unsigned int width, unsigned int height) {
  WINDOW_WIDTH = width;
  WINDOW_HEIGHT = height;
  const unsigned int minDim = std::min(width, height);
  if (minDim > WINDOW_MARGIN * 2) {
    WINDOW_PX_SIZE = minDim - WINDOW_MARGIN * 2;
  } else {
    WINDOW_PX_SIZE = minDim;
  }
  BOARD_OFFSET_X = (width > WINDOW_PX_SIZE) ? (width - WINDOW_PX_SIZE) / 2 : 0;
  BOARD_OFFSET_Y = (height > WINDOW_PX_SIZE) ? (height - WINDOW_PX_SIZE) / 2 : 0;
  SQUARE_PX_SIZE = WINDOW_PX_SIZE / BOARD_SIZE;
  ATTACK_DOT_PX_SIZE =
      static_cast<unsigned int>(static_cast<float>(SQUARE_PX_SIZE) * 0.45f + 0.5f);
  CAPTURE_CIRCLE_PX_SIZE =
      static_cast<unsigned int>(static_cast<float>(SQUARE_PX_SIZE) * 1.02f + 0.5f);
  EVAL_BAR_HEIGHT = WINDOW_PX_SIZE;
  EVAL_BAR_WIDTH = BOARD_OFFSET_X;
  HOVER_PX_SIZE = SQUARE_PX_SIZE;
}

}  // namespace lilia::view::constant
