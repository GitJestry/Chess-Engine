#pragma once

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>

#include <vector>

#include "entity.hpp"
#include "lilia/chess_types.hpp"
#include "lilia/player_info.hpp"
#include "color_palette_manager.hpp"

namespace lilia::view {

class PlayerInfoView {
 public:
 PlayerInfoView();
  ~PlayerInfoView();

  void setInfo(const PlayerInfo& info);
  void setPlayerColor(core::Color color);
  void setPositionClamped(const Entity::Position& pos, const sf::Vector2u& viewportSize);
  void render(sf::RenderWindow& window);

  void addCapturedPiece(core::PieceType type, core::Color color);
  void removeCapturedPiece();
  void clearCapturedPieces();

 private:
  void setPosition(const Entity::Position& pos);
  Entity m_icon;
  sf::RectangleShape m_frame;
  sf::Font m_font;
  sf::Font m_font2;
  sf::Text m_name;
  sf::Text m_elo;
  sf::RectangleShape m_captureBox;
  sf::Text m_noCaptures;
  core::Color m_playerColor{core::Color::White};
  Entity::Position m_position{};
  std::vector<Entity> m_capturedPieces;
  std::vector<std::pair<core::PieceType, core::Color>> m_capturedInfo;
  std::string m_iconPath;

  ColorPaletteManager::ListenerID m_listener_id{0};

  void applyTheme();

  void layoutCaptured();
};

}  // namespace lilia::view
