#include "lilia/view/color_palette_manager.hpp"

namespace lilia::view {

ColorPaletteManager& ColorPaletteManager::get() {
  static ColorPaletteManager instance;
  return instance;
}

ColorPaletteManager::ColorPaletteManager() {
#define X(name, defaultValue) \
  m_default.name = defaultValue; \
  m_current.name = defaultValue;
  LILIA_COLOR_PALETTE(X)
#undef X

  registerPalette("default", ColorPalette{});

  ColorPalette red;
  red.COL_BOARD_LIGHT = sf::Color(255, 200, 200);
  red.COL_BOARD_DARK = sf::Color(160, 50, 50);
  red.COL_ACCENT = sf::Color(200, 60, 60);
  registerPalette("red stream", red);

  m_active = "default";
}

void ColorPaletteManager::registerPalette(const std::string& name, const ColorPalette& palette) {
  if (!m_palettes.count(name)) m_order.push_back(name);
  m_palettes[name] = palette;
}

void ColorPaletteManager::setPalette(const std::string& name) {
  auto it = m_palettes.find(name);
  if (it != m_palettes.end()) {
    loadPalette(it->second);
    m_active = name;
  }
}

void ColorPaletteManager::loadPalette(const ColorPalette& palette) {
#define X(name, defaultValue) m_current.name = palette.name.value_or(m_default.name);
  LILIA_COLOR_PALETTE(X)
#undef X
}

}  // namespace lilia::view
