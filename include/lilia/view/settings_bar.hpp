#pragma once

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

#include "entity.hpp"
#include "../controller/mousepos.hpp"

namespace lilia::view {

class SettingsBar {
 public:
  SettingsBar();

  void init();
  void setBoardPosition(const Entity::Position& boardCenter);
  void render(sf::RenderWindow& window);
  [[nodiscard]] bool isOnFlipIcon(core::MousePos mousePos) const;

 private:
  sf::RectangleShape m_background;
  sf::RectangleShape m_icon_highlight;
  Entity m_flip_icon;
};

}  // namespace lilia::view
