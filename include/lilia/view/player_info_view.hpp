#pragma once

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>

#include "entity.hpp"
#include "lilia/player_info.hpp"

namespace lilia::view {

class PlayerInfoView {
 public:
  PlayerInfoView();

  void setInfo(const PlayerInfo& info);
  void setPositionClamped(const Entity::Position& pos, const sf::Vector2u& viewportSize);
  void render(sf::RenderWindow& window);

 private:
  void setPosition(const Entity::Position& pos);
  Entity m_icon;
  sf::RectangleShape m_frame;
  sf::Font m_font;
  sf::Font m_font2;
  sf::Text m_name;
  sf::Text m_elo;
  Entity::Position m_position{};
};

}  // namespace lilia::view
