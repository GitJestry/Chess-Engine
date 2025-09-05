#pragma once
#include <SFML/Graphics/Color.hpp>
#include <optional>

namespace lilia::view {

// Amethyst Lotus — a more creative, high-contrast violet theme
// Idea: lavender light squares + indigo-slate dark squares,
// deep charcoal UI, amethyst primary accent, warm amber last-move,
// cool cyan premove, mint VALID. Designed to avoid “samey purple” mush.

// Macro listing all color palette entries with their default values
#define LILIA_COLOR_PALETTE(X)                                                               \
  X(COL_EVAL_WHITE, sf::Color(255, 255, 255))             /* crisp */                        \
  X(COL_EVAL_BLACK, sf::Color(49, 42, 61))                /* eggplant */                     \
  X(COL_BOARD_LIGHT, sf::Color(245, 240, 252))            /* #F5F0FC lavender */             \
  X(COL_BOARD_DARK, sf::Color(70, 79, 130))               /* #464F82 indigo-slate */         \
  X(COL_SELECT_HIGHLIGHT, sf::Color(255, 211, 110, 170))  /* #FFD36E amber */                \
  X(COL_PREMOVE_HIGHLIGHT, sf::Color(96, 196, 255, 160))  /* #60C4FF cyan */                 \
  X(COL_WARNING_HIGHLIGHT, sf::Color(255, 122, 136, 190)) /* #FF7A88 */                      \
  X(COL_RCLICK_HIGHLIGHT, sf::Color(162, 117, 255, 170))  /* amethyst ping */                \
  X(COL_HOVER_OUTLINE, sf::Color(238, 233, 255, 110))     /* soft glow */                    \
  X(COL_MARKER, sf::Color(162, 117, 255, 65))             /* subtle dot */                   \
  X(COL_PANEL, sf::Color(26, 23, 34, 230))                /* #1A1722 deep charcoal-violet */ \
  X(COL_HEADER, sf::Color(40, 34, 54))                    /* #282236 */                      \
  X(COL_SIDEBAR_BG, sf::Color(20, 18, 28))                /* #14121C */                      \
  X(COL_LIST_BG, sf::Color(24, 22, 33))                   /* #181621 */                      \
  X(COL_ROW_EVEN, sf::Color(31, 28, 41))                  /* #1F1C29 */                      \
  X(COL_ROW_ODD, sf::Color(27, 24, 36))                   /* #1B1824 */                      \
  X(COL_HOVER_BG, sf::Color(55, 50, 74))                  /* #37324A */                      \
  X(COL_TEXT, sf::Color(242, 240, 252))                   /* #F2F0FC */                      \
  X(COL_MUTED_TEXT, sf::Color(194, 192, 212))             /* #C2C0D4 */                      \
  X(COL_ACCENT, sf::Color(162, 117, 255))                 /* #A273FF amethyst */             \
  X(COL_ACCENT_HOVER, sf::Color(187, 153, 255))           /* #BB99FF */                      \
  X(COL_ACCENT_OUTLINE, sf::Color(162, 117, 255, 90))                                        \
  X(COL_SLOT_BASE, sf::Color(44, 39, 56))       /* #2C2738 */                                \
  X(COL_DARK_TEXT, sf::Color(23, 19, 30))       /* #17131E */                                \
  X(COL_LIGHT_TEXT, sf::Color(250, 248, 255))   /* #FAF8FF */                                \
  X(COL_LIGHT_BG, sf::Color(224, 218, 240))     /* #E0DAF0 */                                \
  X(COL_DARK_BG, sf::Color(18, 16, 24))         /* #121018 */                                \
  X(COL_CLOCK_ACCENT, sf::Color(245, 242, 255)) /* #F5F2FF */                                \
  X(COL_TOOLTIP_BG, sf::Color(22, 19, 30, 230)) /* #16131E */                                \
  X(COL_DISC, sf::Color(58, 49, 74, 150))                                                    \
  X(COL_DISC_HOVER, sf::Color(67, 56, 90, 180))                                              \
  X(COL_BORDER, sf::Color(156, 162, 204, 60)) /* #9CA2CC periwinkle steel */                 \
  X(COL_BORDER_LIGHT, sf::Color(156, 162, 204, 50))                                          \
  X(COL_BORDER_BEVEL, sf::Color(156, 162, 204, 40))                                          \
  X(COL_BOARD_OUTLINE, sf::Color(94, 72, 158, 120)) /* #5E489E */                            \
  X(COL_SHADOW_LIGHT, sf::Color(0, 0, 0, 60))                                                \
  X(COL_SHADOW_MEDIUM, sf::Color(0, 0, 0, 90))                                               \
  X(COL_SHADOW_STRONG, sf::Color(0, 0, 0, 140))                                              \
  X(COL_SHADOW_BAR, sf::Color(0, 0, 0, 70))                                                  \
  X(COL_MOVE_HIGHLIGHT, sf::Color(162, 117, 255, 48))                                        \
  X(COL_OVERLAY_DIM, sf::Color(0, 0, 0, 100))                                                \
  X(COL_OVERLAY, sf::Color(0, 0, 0, 120))                                                    \
  X(COL_GOLD, sf::Color(212, 175, 55))                                                       \
  X(COL_WHITE_DIM, sf::Color(255, 255, 255, 70))                                             \
  X(COL_WHITE_FAINT, sf::Color(255, 255, 255, 30))                                           \
  X(COL_SCORE_TEXT_DARK, sf::Color(20, 18, 28))     /* #14121C */                            \
  X(COL_SCORE_TEXT_LIGHT, sf::Color(236, 232, 250)) /* #ECE8FA */                            \
  X(COL_LOW_TIME, sf::Color(220, 70, 70))                                                    \
  X(COL_BG_TOP, sf::Color(28, 24, 35))    /* #1C1823 */                                      \
  X(COL_BG_BOTTOM, sf::Color(16, 13, 22)) /* #100D16 */                                      \
  X(COL_PANEL_TRANS, sf::Color(26, 23, 34, 150))                                             \
  X(COL_PANEL_BORDER_ALT, sf::Color(200, 194, 224, 50)) /* #C8C2E0 */                        \
  X(COL_BUTTON, sf::Color(54, 46, 74))                  /* #362E4A */                        \
  X(COL_BUTTON_ACTIVE, sf::Color(127, 106, 210))        /* #7F6AD2 */                        \
  X(COL_TIME_OFF, sf::Color(112, 80, 168))              /* #7050A8 */                        \
  X(COL_INPUT_BORDER, sf::Color(186, 178, 220))         /* #BAB2DC */                        \
  X(COL_INPUT_BG, sf::Color(42, 35, 56))                /* #2A2338 */                        \
  X(COL_VALID, sf::Color(122, 205, 164))                /* mint OK */                        \
  X(COL_INVALID, sf::Color(217, 106, 134))              /* rose error */                     \
  X(COL_LOGO_BG, sf::Color(162, 117, 255, 70))                                               \
  X(COL_TOP_HILIGHT, sf::Color(255, 255, 255, 18))                                           \
  X(COL_BOTTOM_SHADOW, sf::Color(0, 0, 0, 40))                                               \
  X(COL_PANEL_ALPHA220, sf::Color(26, 23, 34, 220))

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
