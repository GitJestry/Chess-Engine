#include "lilia/view/col_palette/chess_com.hpp"

namespace lilia::view {

// Chess.com — green & parchment board, brand-green accents, dark neutral UI
const ColorPalette& chessComPalette() {
  static const ColorPalette palette = [] {
    ColorPalette p{};

    // Board
    p.COL_BOARD_LIGHT = sf::Color(238, 238, 210);      // #EEEED2  (parchment)
    p.COL_BOARD_DARK = sf::Color(118, 150, 86);        // #769656  (classic green)
    p.COL_BOARD_OUTLINE = sf::Color(63, 74, 60, 120);  // #3F4A3C with alpha

    // Accents & interactive
    p.COL_ACCENT = sf::Color(92, 171, 60);                   // #5CAB3C (brand green)
    p.COL_ACCENT_HOVER = sf::Color(107, 193, 75);            // brighter green
    p.COL_ACCENT_OUTLINE = sf::Color(92, 171, 60, 90);       // soft green outline
    p.COL_SELECT_HIGHLIGHT = sf::Color(92, 171, 60, 170);    // on-brand select
    p.COL_PREMOVE_HIGHLIGHT = sf::Color(74, 144, 226, 160);  // #4A90E2 (premove blue)
    p.COL_WARNING_HIGHLIGHT = sf::Color(230, 126, 34, 190);  // #E67E22 (warning)
    p.COL_RCLICK_HIGHLIGHT = sf::Color(92, 171, 60, 170);    // green ping
    p.COL_HOVER_OUTLINE = sf::Color(255, 246, 204, 110);     // warm light outline
    p.COL_MOVE_HIGHLIGHT = sf::Color(92, 171, 60, 48);       // subtle green wash
    p.COL_MARKER = sf::Color(92, 171, 60, 65);               // quiet marker

    // Text
    p.COL_TEXT = sf::Color(250, 250, 247);        // near-white
    p.COL_MUTED_TEXT = sf::Color(200, 212, 195);  // soft sage gray
    p.COL_LIGHT_TEXT = sf::Color(255, 255, 255);
    p.COL_DARK_TEXT = sf::Color(15, 18, 13);

    // Evaluation bars
    p.COL_EVAL_WHITE = sf::Color(255, 255, 255);
    p.COL_EVAL_BLACK = sf::Color(30, 31, 27);

    // Panels & chrome (dark, slightly green-tinted neutrals)
    p.COL_PANEL = sf::Color(24, 30, 22, 230);
    p.COL_HEADER = sf::Color(47, 58, 44);  // #2F3A2C
    p.COL_SIDEBAR_BG = sf::Color(18, 22, 16);
    p.COL_LIST_BG = sf::Color(20, 24, 18);
    p.COL_ROW_EVEN = sf::Color(24, 28, 22);
    p.COL_ROW_ODD = sf::Color(22, 26, 20);
    p.COL_HOVER_BG = sf::Color(38, 46, 34);
    p.COL_SLOT_BASE = sf::Color(38, 44, 36);
    p.COL_BUTTON = sf::Color(40, 48, 38);
    p.COL_BUTTON_ACTIVE = sf::Color(70, 84, 66);
    p.COL_PANEL_TRANS = sf::Color(24, 30, 22, 150);
    p.COL_PANEL_BORDER_ALT = sf::Color(192, 220, 188, 50);

    // Backgrounds / gradients
    p.COL_LIGHT_BG = sf::Color(232, 241, 224);  // soft paper green
    p.COL_DARK_BG = sf::Color(12, 16, 10);
    p.COL_BG_TOP = sf::Color(10, 13, 9);
    p.COL_BG_BOTTOM = sf::Color(7, 10, 6);

    // Tooltip, discs, borders
    p.COL_TOOLTIP_BG = sf::Color(12, 16, 11, 230);
    p.COL_DISC = sf::Color(34, 42, 32, 150);
    p.COL_DISC_HOVER = sf::Color(40, 50, 38, 180);
    p.COL_BORDER = sf::Color(170, 190, 160, 60);
    p.COL_BORDER_LIGHT = sf::Color(170, 190, 160, 50);
    p.COL_BORDER_BEVEL = sf::Color(170, 190, 160, 40);

    // Inputs
    p.COL_INPUT_BG = sf::Color(28, 34, 28);
    p.COL_INPUT_BORDER = sf::Color(150, 180, 145);

    // Time, score, misc
    p.COL_CLOCK_ACCENT = sf::Color(250, 255, 245);
    p.COL_TIME_OFF = sf::Color(139, 46, 46);  // muted red for low time
    p.COL_SCORE_TEXT_DARK = sf::Color(12, 16, 11);
    p.COL_SCORE_TEXT_LIGHT = sf::Color(244, 255, 240);
    p.COL_INVALID = sf::Color(194, 58, 58);  // semantic error

    // Brand / logo & overlays
    p.COL_LOGO_BG = sf::Color(92, 171, 60, 70);  // brand green wash
    p.COL_TOP_HILIGHT = sf::Color(255, 255, 255, 18);
    p.COL_BOTTOM_SHADOW = sf::Color(0, 0, 0, 40);
    p.COL_PANEL_ALPHA220 = sf::Color(24, 30, 22, 220);

    // Shadows
    p.COL_SHADOW_LIGHT = sf::Color(0, 0, 0, 60);
    p.COL_SHADOW_MEDIUM = sf::Color(0, 0, 0, 90);
    p.COL_SHADOW_STRONG = sf::Color(0, 0, 0, 140);
    p.COL_SHADOW_BAR = sf::Color(0, 0, 0, 70);

    // Overlays
    p.COL_OVERLAY_DIM = sf::Color(0, 0, 0, 100);
    p.COL_OVERLAY = sf::Color(0, 0, 0, 120);

    // Leave GOLD/WHITE_* and VALID as defaults unless you want to lock them to theme hues.
    return p;
  }();

  return palette;
}

}  // namespace lilia::view
