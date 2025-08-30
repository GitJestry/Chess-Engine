#pragma once

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>
#include <string>

namespace lilia::view {

class PlayerInfoView {
 public:
  PlayerInfoView();

  void setNames(const std::string& white, const std::string& black);
  void layout(float boardCenterX, float boardCenterY, float boardHalf);
  void render(sf::RenderWindow& window);

 private:
  sf::Font m_font;
  sf::Text m_white_text;
  sf::Text m_black_text;
};

}  // namespace lilia::view

