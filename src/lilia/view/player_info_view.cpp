#include "lilia/view/player_info_view.hpp"

#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view {

PlayerInfoView::PlayerInfoView() {
  m_frame.setFillColor(sf::Color(60, 60, 60));
  m_frame.setOutlineColor(sf::Color(120, 120, 120));
  m_frame.setOutlineThickness(2.f);
  if (m_font.loadFromFile(constant::STR_FILE_PATH_FONT)) {
    m_text.setFont(m_font);
    m_text.setCharacterSize(18);
    m_text.setFillColor(sf::Color::White);
  }
}

void PlayerInfoView::setInfo(const PlayerInfo& info) {
  m_icon.setTexture(TextureTable::getInstance().get(info.iconPath));
  auto size = m_icon.getOriginalSize();
  if (size.x > 0) {
    float scale = 32.f / static_cast<float>(size.x);
    m_icon.setScale(scale, scale);
  }
  m_icon.setOriginToCenter();
  m_text.setString(info.name + " (" + std::to_string(info.elo) + ")");
  auto bounds = m_text.getLocalBounds();
  m_frame.setSize({bounds.width + 48.f, 40.f});
}

void PlayerInfoView::setPosition(const Entity::Position& pos) {
  m_position = pos;
  m_frame.setPosition(pos);
  m_icon.setPosition({pos.x + 20.f, pos.y + 20.f});
  m_text.setPosition(pos.x + 40.f, pos.y + 8.f);
}

void PlayerInfoView::render(sf::RenderWindow& window) {
  window.draw(m_frame);
  m_icon.draw(window);
  window.draw(m_text);
}

}  // namespace lilia::view
