#pragma once
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Text.hpp>
#include <string>

#include "../controller/mousepos.hpp"
#include "color_palette_manager.hpp"
#include "entity.hpp"

namespace sf {
class RenderWindow;
}

namespace lilia::view {

class EvalBar : Entity {
 public:
  EvalBar();
  ~EvalBar();

  virtual void setPosition(const Entity::Position &pos) override;
  void render(sf::RenderWindow &window);
  void update(int eval);
  void setResult(const std::string &result);
  void reset();

  void toggleVisibility();
  [[nodiscard]] bool isOnToggle(core::MousePos mousePos) const;

  void setFlipped(bool flipped);

 private:
  void scaleToEval(float e);
  Entity m_black_background;
  Entity m_white_fill_eval;
  sf::Font m_font;
  sf::Text m_score_text;
  sf::Text m_toggle_text;
  sf::FloatRect m_toggle_bounds;
  bool m_visible{false};
  float m_display_eval{0.f};
  float m_target_eval{0.f};
  bool m_has_result{false};
  std::string m_result;
  bool m_flipped{false};

  ColorPaletteManager::ListenerID m_paletteListener{0};
  void onPaletteChanged();
};

}  // namespace lilia::view
