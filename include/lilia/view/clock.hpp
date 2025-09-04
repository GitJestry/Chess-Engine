#pragma once

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>

#include "lilia/chess_types.hpp"
#include "lilia/view/render_constants.hpp"

namespace lilia::view {

class Clock {
 public:
  Clock();

  void setPlayerColor(core::Color color);
  void setPosition(const sf::Vector2f& pos);
  void setTime(float seconds);
  void setActive(bool active);
  void render(sf::RenderWindow& window);

  static constexpr float WIDTH = 144.f;
  static constexpr float HEIGHT = 40.f;

 private:
  sf::RectangleShape m_box;
  sf::RectangleShape m_overlay;
  sf::Text m_text;
  sf::Font m_font;
  // Remember the player's base theme/text color so low-time highlighting
  // can temporarily override it without losing the original.
  sf::Color m_text_base_color;
  // Remember the base fill color so low-time warning can temporarily
  // override it without losing the original.
  sf::Color m_box_base_color;
  bool m_low_time{false};
  bool m_active{false};
  bool m_is_light_theme{false};
  sf::CircleShape m_icon_circle;
  sf::RectangleShape m_icon_hand;

  void applyFillColor();
};

}  // namespace lilia::view

