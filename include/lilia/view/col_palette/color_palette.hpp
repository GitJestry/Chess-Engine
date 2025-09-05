#pragma once
#include <SFML/Graphics/Color.hpp>
#include <optional>

namespace lilia::view {

// Amethyst Lotus — lavender board, amethyst accents, deep eggplant UI
// (Diese Defaults können als "brand default" genutzt werden.)
#define LILIA_COLOR_PALETTE(X)                                           \
  X(COL_EVAL_WHITE, sf::Color(255, 255, 255))                            \
  X(COL_EVAL_BLACK, sf::Color(49, 42, 61))               /* #312A3D */   \
  X(COL_BOARD_LIGHT, sf::Color(241, 236, 250))           /* #F1ECFA */   \
  X(COL_BOARD_DARK, sf::Color(108, 94, 153))             /* #6C5E99 */   \
  X(COL_SELECT_HIGHLIGHT, sf::Color(246, 224, 122, 170)) /* last move */ \
  X(COL_PREMOVE_HIGHLIGHT, sf::Color(138, 167, 255, 160))                \
  X(COL_WARNING_HIGHLIGHT, sf::Color(232, 106, 118, 200))                \
  X(COL_RCLICK_HIGHLIGHT, sf::Color(162, 117, 255, 170))                 \
  X(COL_HOVER_OUTLINE, sf::Color(238, 233, 255, 110))                    \
  X(COL_MARKER, sf::Color(162, 117, 255, 65))                            \
  X(COL_PANEL, sf::Color(31, 26, 39, 230))                               \
  X(COL_HEADER, sf::Color(42, 36, 53))                                   \
  X(COL_SIDEBAR_BG, sf::Color(26, 21, 34))                               \
  X(COL_LIST_BG, sf::Color(33, 27, 42))                                  \
  X(COL_ROW_EVEN, sf::Color(37, 31, 47))                                 \
  X(COL_ROW_ODD, sf::Color(32, 27, 39))                                  \
  X(COL_HOVER_BG, sf::Color(58, 49, 74))                                 \
  X(COL_TEXT, sf::Color(238, 233, 255))                                  \
  X(COL_MUTED_TEXT, sf::Color(200, 194, 224))                            \
  X(COL_ACCENT, sf::Color(162, 117, 255))                                \
  X(COL_ACCENT_HOVER, sf::Color(187, 153, 255))                          \
  X(COL_ACCENT_OUTLINE, sf::Color(162, 117, 255, 90))                    \
  X(COL_SLOT_BASE, sf::Color(43, 36, 56))                                \
  X(COL_DARK_TEXT, sf::Color(23, 19, 30))                                \
  X(COL_LIGHT_TEXT, sf::Color(250, 248, 255))                            \
  X(COL_LIGHT_BG, sf::Color(220, 213, 238))                              \
  X(COL_DARK_BG, sf::Color(23, 19, 31))                                  \
  X(COL_CLOCK_ACCENT, sf::Color(243, 241, 251))                          \
  X(COL_TOOLTIP_BG, sf::Color(23, 19, 33, 230))                          \
  X(COL_DISC, sf::Color(58, 49, 74, 150))                                \
  X(COL_DISC_HOVER, sf::Color(67, 56, 90, 180))                          \
  X(COL_BORDER, sf::Color(169, 161, 200, 60))                            \
  X(COL_BORDER_LIGHT, sf::Color(169, 161, 200, 50))                      \
  X(COL_BORDER_BEVEL, sf::Color(169, 161, 200, 40))                      \
  X(COL_BOARD_OUTLINE, sf::Color(94, 72, 158, 120)) /* #5E489E */        \
  X(COL_SHADOW_LIGHT, sf::Color(0, 0, 0, 60))                            \
  X(COL_SHADOW_MEDIUM, sf::Color(0, 0, 0, 90))                           \
  X(COL_SHADOW_STRONG, sf::Color(0, 0, 0, 140))                          \
  X(COL_SHADOW_BAR, sf::Color(0, 0, 0, 70))                              \
  X(COL_MOVE_HIGHLIGHT, sf::Color(162, 117, 255, 48))                    \
  X(COL_OVERLAY_DIM, sf::Color(0, 0, 0, 100))                            \
  X(COL_OVERLAY, sf::Color(0, 0, 0, 120))                                \
  X(COL_GOLD, sf::Color(212, 175, 55))                                   \
  X(COL_WHITE_DIM, sf::Color(255, 255, 255, 70))                         \
  X(COL_WHITE_FAINT, sf::Color(255, 255, 255, 30))                       \
  X(COL_SCORE_TEXT_DARK, sf::Color(18, 14, 24))                          \
  X(COL_SCORE_TEXT_LIGHT, sf::Color(236, 232, 250))                      \
  X(COL_LOW_TIME, sf::Color(220, 70, 70))                                \
  X(COL_BG_TOP, sf::Color(28, 24, 35))                                   \
  X(COL_BG_BOTTOM, sf::Color(16, 13, 22))                                \
  X(COL_PANEL_TRANS, sf::Color(31, 26, 39, 150))                         \
  X(COL_PANEL_BORDER_ALT, sf::Color(200, 194, 224, 50))                  \
  X(COL_BUTTON, sf::Color(52, 44, 70))                                   \
  X(COL_BUTTON_ACTIVE, sf::Color(124, 102, 201))                         \
  X(COL_TIME_OFF, sf::Color(110, 74, 163))                               \
  X(COL_INPUT_BORDER, sf::Color(183, 176, 214))                          \
  X(COL_INPUT_BG, sf::Color(40, 33, 54))                                 \
  X(COL_VALID, sf::Color(86, 180, 130))                                  \
  X(COL_INVALID, sf::Color(217, 106, 134))                               \
  X(COL_LOGO_BG, sf::Color(162, 117, 255, 70))                           \
  X(COL_TOP_HILIGHT, sf::Color(255, 255, 255, 18))                       \
  X(COL_BOTTOM_SHADOW, sf::Color(0, 0, 0, 40))                           \
  X(COL_PANEL_ALPHA220, sf::Color(31, 26, 39, 220))

struct ColorPalette {
#define X(name, defaultValue) std::optional<sf::Color> name;
  LILIA_COLOR_PALETTE(X)
#undef X
};

struct PaletteColors {
#define X(name, defaultValue) sf::Color name;
  LILIA_COLOR_PALETTE(X)
#undef X
};

}  // namespace lilia::view
