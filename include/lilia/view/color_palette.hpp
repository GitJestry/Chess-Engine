#pragma once
#include <SFML/Graphics/Color.hpp>
#include <optional>

namespace lilia::view {

// Macro listing all color palette entries with their default values
#define LILIA_COLOR_PALETTE(X) \
    X(COL_EVAL_WHITE, sf::Color(236, 240, 255)) \
    X(COL_EVAL_BLACK, sf::Color(42, 48, 63)) \
    X(COL_BOARD_LIGHT, sf::Color(230, 235, 244)) \
    X(COL_BOARD_DARK, sf::Color(94, 107, 135)) \
    X(COL_SELECT_HIGHLIGHT, sf::Color(100, 190, 255, 170)) \
    X(COL_PREMOVE_HIGHLIGHT, sf::Color(180, 120, 255, 160)) \
    X(COL_WARNING_HIGHLIGHT, sf::Color(255, 102, 102, 190)) \
    X(COL_RCLICK_HIGHLIGHT, sf::Color(255, 80, 80, 170)) \
    X(COL_HOVER_OUTLINE, sf::Color(180, 220, 120, 110)) \
    X(COL_MARKER, sf::Color(120, 120, 120, 65)) \
    X(COL_PANEL, sf::Color(36, 41, 54, 230)) \
    X(COL_HEADER, sf::Color(42, 48, 63)) \
    X(COL_SIDEBAR_BG, sf::Color(36, 41, 54)) \
    X(COL_LIST_BG, sf::Color(33, 38, 50)) \
    X(COL_ROW_EVEN, sf::Color(44, 50, 66)) \
    X(COL_ROW_ODD, sf::Color(38, 44, 58)) \
    X(COL_HOVER_BG, sf::Color(58, 66, 84)) \
    X(COL_TEXT, sf::Color(240, 244, 255)) \
    X(COL_MUTED_TEXT, sf::Color(180, 186, 205)) \
    X(COL_ACCENT, sf::Color(100, 190, 255)) \
    X(COL_ACCENT_HOVER, sf::Color(120, 205, 255)) \
    X(COL_ACCENT_OUTLINE, sf::Color(140, 200, 240, 90)) \
    X(COL_SLOT_BASE, sf::Color(50, 56, 72)) \
    X(COL_DARK_TEXT, sf::Color(26, 22, 30)) \
    X(COL_LIGHT_TEXT, sf::Color(210, 224, 255)) \
    X(COL_LIGHT_BG, sf::Color(210, 215, 230)) \
    X(COL_DARK_BG, sf::Color(33, 38, 50)) \
    X(COL_CLOCK_ACCENT, sf::Color(225, 225, 235)) \
    X(COL_TOOLTIP_BG, sf::Color(20, 24, 32, 230)) \
    X(COL_DISC, sf::Color(52, 58, 74, 150)) \
    X(COL_DISC_HOVER, sf::Color(60, 68, 86, 180)) \
    X(COL_BORDER, sf::Color(120, 140, 170, 60)) \
    X(COL_BORDER_LIGHT, sf::Color(120, 140, 170, 50)) \
    X(COL_BORDER_BEVEL, sf::Color(120, 140, 170, 40)) \
    X(COL_BOARD_OUTLINE, sf::Color(52, 58, 74, 120)) \
    X(COL_SHADOW_LIGHT, sf::Color(0, 0, 0, 60)) \
    X(COL_SHADOW_MEDIUM, sf::Color(0, 0, 0, 90)) \
    X(COL_SHADOW_STRONG, sf::Color(0, 0, 0, 140)) \
    X(COL_SHADOW_BAR, sf::Color(0, 0, 0, 70)) \
    X(COL_MOVE_HIGHLIGHT, sf::Color(80, 100, 120, 40)) \
    X(COL_OVERLAY_DIM, sf::Color(0, 0, 0, 100)) \
    X(COL_OVERLAY, sf::Color(0, 0, 0, 120)) \
    X(COL_GOLD, sf::Color(212, 175, 55)) \
    X(COL_WHITE_DIM, sf::Color(255, 255, 255, 70)) \
    X(COL_WHITE_FAINT, sf::Color(255, 255, 255, 30)) \
    X(COL_SCORE_TEXT_DARK, sf::Color(20, 20, 26)) \
    X(COL_SCORE_TEXT_LIGHT, sf::Color(230, 238, 255)) \
    X(COL_LOW_TIME, sf::Color(220, 70, 70)) \
    X(COL_BG_TOP, sf::Color(24, 29, 38)) \
    X(COL_BG_BOTTOM, sf::Color(16, 19, 26)) \
    X(COL_PANEL_TRANS, sf::Color(36, 41, 54, 150)) \
    X(COL_PANEL_BORDER_ALT, sf::Color(180, 186, 205, 50)) \
    X(COL_BUTTON, sf::Color(58, 64, 80)) \
    X(COL_BUTTON_ACTIVE, sf::Color(92, 98, 120)) \
    X(COL_TIME_OFF, sf::Color(86, 64, 96)) \
    X(COL_INPUT_BORDER, sf::Color(120, 140, 180)) \
    X(COL_INPUT_BG, sf::Color(44, 50, 66)) \
    X(COL_VALID, sf::Color(86, 180, 130)) \
    X(COL_INVALID, sf::Color(220, 90, 90)) \
    X(COL_LOGO_BG, sf::Color(150, 120, 255, 70)) \
    X(COL_TOP_HILIGHT, sf::Color(255, 255, 255, 18)) \
    X(COL_BOTTOM_SHADOW, sf::Color(0, 0, 0, 40)) \
    X(COL_PANEL_ALPHA220, sf::Color(36, 41, 54, 220))

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

