#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

#include "col_palette/color_palette.hpp"

namespace lilia::view {

class ColorPaletteManager {
 public:
  static ColorPaletteManager& get();

  // Register a named palette that can be selected later
  void registerPalette(const std::string& name, const ColorPalette& palette);

  // Activate a palette by name
  void setPalette(const std::string& name);

  // Load a new palette directly; unspecified colors fall back to defaults
  void loadPalette(const ColorPalette& palette);

  // Access the active palette
  const PaletteColors& palette() const { return m_current; }
  PaletteColors& palette() { return m_current; }
  const PaletteColors& defaultPalette() const { return m_default; }
  const std::vector<std::string>& paletteNames() const { return m_order; }
  const std::string& activePalette() const { return m_active; }

  using ListenerID = std::size_t;
  ListenerID addListener(std::function<void()> listener);
  void removeListener(ListenerID id);

 private:
  ColorPaletteManager();

  PaletteColors m_default;
  PaletteColors m_current;
  std::unordered_map<std::string, ColorPalette> m_palettes;
  std::vector<std::string> m_order;
  std::string m_active;
  std::unordered_map<ListenerID, std::function<void()>> m_listeners;
  ListenerID m_nextListenerId{0};
};

}  // namespace lilia::view
