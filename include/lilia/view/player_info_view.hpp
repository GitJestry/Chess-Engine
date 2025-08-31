#pragma once

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>

#include "lilia/player_info.hpp"
#include "entity.hpp"

namespace lilia::view {

class PlayerInfoView {
 public:
  PlayerInfoView();

  void setInfo(const PlayerInfo& info);
  void setPosition(const Entity::Position& pos);
  void render(sf::RenderWindow& window) const;

 private:
  Entity m_icon;
  sf::RectangleShape m_frame;
  sf::Font m_font;
  sf::Text m_text;
  Entity::Position m_position{};
};

} // namespace lilia::view

