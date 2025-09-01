#pragma once

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
  void render(sf::RenderWindow& window);

  static constexpr float WIDTH = 120.f;
  static constexpr float HEIGHT = 40.f;

 private:
  sf::RectangleShape m_box;
  sf::Text m_text;
  sf::Font m_font;
};

}  // namespace lilia::view

