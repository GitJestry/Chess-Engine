#include "lilia/view/col_palette/rose_noir.hpp"

namespace lilia::view {

// Rose Noir — a burgundy/charcoal dark theme with warm ivory lights
const ColorPalette& roseNoirPalette() {
  static const ColorPalette palette = [] {
    ColorPalette p{};

    // Board
    p.COL_BOARD_LIGHT = sf::Color(248, 240, 242);      // #F8F0F2  (≈ between FFFFFA and 912F40)
    p.COL_BOARD_DARK = sf::Color(88, 52, 64);          // #583440  (mix of 40434E & 702632)
    p.COL_BOARD_OUTLINE = sf::Color(64, 67, 78, 120);  // #40434E with alpha

    // Accents & interactive
    p.COL_ACCENT = sf::Color(145, 47, 64);                   // #912F40
    p.COL_ACCENT_HOVER = sf::Color(170, 66, 83);             // lighter burgundy
    p.COL_ACCENT_OUTLINE = sf::Color(145, 47, 64, 90);       // soft burgundy outline
    p.COL_SELECT_HIGHLIGHT = sf::Color(200, 120, 135, 170);  // rosy select
    p.COL_PREMOVE_HIGHLIGHT = sf::Color(165, 88, 110, 160);  // wine premove
    p.COL_WARNING_HIGHLIGHT = sf::Color(200, 80, 95, 190);   // warm warning (still on-brand)
    p.COL_RCLICK_HIGHLIGHT = sf::Color(145, 47, 64, 170);    // burgundy ping
    p.COL_HOVER_OUTLINE = sf::Color(255, 235, 240, 110);     // warm ivory outline
    p.COL_MOVE_HIGHLIGHT = sf::Color(145, 47, 64, 48);       // subtle burgundy wash
    p.COL_MARKER = sf::Color(145, 47, 64, 65);               // quiet marker

    // Text
    p.COL_TEXT = sf::Color(255, 252, 250);        // #FFFCEA
    p.COL_MUTED_TEXT = sf::Color(200, 192, 200);  // warm gray
    p.COL_LIGHT_TEXT = sf::Color(255, 250, 248);
    p.COL_DARK_TEXT = sf::Color(16, 14, 18);

    // Evaluation bars
    p.COL_EVAL_WHITE = sf::Color(255, 252, 250);
    p.COL_EVAL_BLACK = sf::Color(30, 28, 32);

    // Panels & chrome
    p.COL_PANEL = sf::Color(24, 25, 30, 230);
    p.COL_HEADER = sf::Color(64, 67, 78);  // #40434E
    p.COL_SIDEBAR_BG = sf::Color(18, 19, 24);
    p.COL_LIST_BG = sf::Color(20, 22, 28);
    p.COL_ROW_EVEN = sf::Color(24, 26, 32);
    p.COL_ROW_ODD = sf::Color(22, 24, 30);
    p.COL_HOVER_BG = sf::Color(38, 40, 48);
    p.COL_SLOT_BASE = sf::Color(38, 36, 44);
    p.COL_BUTTON = sf::Color(40, 42, 50);
    p.COL_BUTTON_ACTIVE = sf::Color(70, 72, 84);
    p.COL_PANEL_TRANS = sf::Color(24, 25, 30, 150);
    p.COL_PANEL_BORDER_ALT = sf::Color(200, 192, 200, 50);

    // Backgrounds / gradients
    p.COL_LIGHT_BG = sf::Color(228, 220, 228);
    p.COL_DARK_BG = sf::Color(14, 12, 16);
    p.COL_BG_TOP = sf::Color(12, 10, 12);  // near #080705 but softer
    p.COL_BG_BOTTOM = sf::Color(8, 7, 5);  // #080705

    // Tooltip, discs, borders
    p.COL_TOOLTIP_BG = sf::Color(12, 10, 14, 230);
    p.COL_DISC = sf::Color(34, 36, 42, 150);
    p.COL_DISC_HOVER = sf::Color(40, 42, 50, 180);
    p.COL_BORDER = sf::Color(170, 150, 160, 60);
    p.COL_BORDER_LIGHT = sf::Color(170, 150, 160, 50);
    p.COL_BORDER_BEVEL = sf::Color(170, 150, 160, 40);

    // Inputs
    p.COL_INPUT_BG = sf::Color(28, 30, 38);
    p.COL_INPUT_BORDER = sf::Color(170, 150, 160);

    // Time, score, misc
    p.COL_CLOCK_ACCENT = sf::Color(252, 245, 245);
    p.COL_TIME_OFF = sf::Color(112, 38, 50);  // #702632
    p.COL_SCORE_TEXT_DARK = sf::Color(14, 12, 16);
    p.COL_SCORE_TEXT_LIGHT = sf::Color(250, 246, 244);
    p.COL_INVALID = sf::Color(145, 47, 64);  // brand-consistent "error"
    // Keep COL_VALID green if you want semantic color; otherwise omit to use default.

    // Brand / logo & overlays
    p.COL_LOGO_BG = sf::Color(145, 47, 64, 70);
    p.COL_TOP_HILIGHT = sf::Color(255, 255, 255, 18);
    p.COL_BOTTOM_SHADOW = sf::Color(0, 0, 0, 40);
    p.COL_PANEL_ALPHA220 = sf::Color(24, 25, 30, 220);

    // Shadows (defaults OK, but we set for completeness)
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

