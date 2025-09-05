#include "lilia/view/color_palette_manager.hpp"

#include "lilia/view/col_palette/rose_noir.hpp"
#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view {

ColorPaletteManager& ColorPaletteManager::get() {
  static ColorPaletteManager instance;
  return instance;
}

ColorPaletteManager::ColorPaletteManager() {
#define X(name, defaultValue)    \
  m_default.name = defaultValue; \
  m_current.name = defaultValue;
  LILIA_COLOR_PALETTE(X)
#undef X

  registerPalette(constant::STR_COL_PALETTE_DEFAULT, ColorPalette{});
  registerPalette(constant::STR_COL_PALETTE_ROSE_NOIR, PALETTE_ROSE_NOIR);

  m_active = constant::STR_COL_PALETTE_DEFAULT;
}

void ColorPaletteManager::registerPalette(const std::string& name, const ColorPalette& palette) {
  if (!m_palettes.count(name)) m_order.push_back(name);
  m_palettes[name] = palette;
}

void ColorPaletteManager::setPalette(const std::string& name) {
  auto it = m_palettes.find(name);
  if (it != m_palettes.end()) {
    loadPalette(it->second);
    TextureTable::getInstance().reloadForPalette();
    m_active = name;
  }
}

void ColorPaletteManager::loadPalette(const ColorPalette& palette) {
#define X(name, defaultValue) m_current.name = palette.name.value_or(m_default.name);
  LILIA_COLOR_PALETTE(X)
#undef X
}

}  // namespace lilia::view
