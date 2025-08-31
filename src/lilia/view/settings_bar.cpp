#include "lilia/view/settings_bar.hpp"

#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view {

SettingsBar::SettingsBar() : m_background(), m_icon_highlight(), m_flip_icon() {}

void SettingsBar::init() {
  m_background.setSize({static_cast<float>(constant::SIDE_MARGIN),
                        static_cast<float>(constant::WINDOW_PX_SIZE)});
  m_background.setFillColor(sf::Color(255, 255, 255, 30));
  m_background.setOutlineColor(sf::Color(200, 200, 200));
  m_background.setOutlineThickness(2.f);

  m_flip_icon.setTexture(TextureTable::getInstance().get(constant::STR_FILE_PATH_FLIP));
  auto size = m_flip_icon.getOriginalSize();
  float scale = (constant::SQUARE_PX_SIZE * 0.3f) / size.x;
  m_flip_icon.setScale(scale, scale);
  m_flip_icon.setOriginToCenter();

  float highlightSize = constant::SQUARE_PX_SIZE * 0.5f;
  m_icon_highlight.setSize({highlightSize, highlightSize});
  m_icon_highlight.setOrigin({highlightSize / 2.f, highlightSize / 2.f});
  m_icon_highlight.setFillColor(sf::Color(0, 0, 0, 0));
  m_icon_highlight.setOutlineColor(sf::Color::White);
  m_icon_highlight.setOutlineThickness(2.f);
}

void SettingsBar::setBoardPosition(const Entity::Position& boardCenter) {
  float boardRight = boardCenter.x + constant::WINDOW_PX_SIZE / 2.f;
  float boardTop = boardCenter.y - constant::WINDOW_PX_SIZE / 2.f;
  m_background.setPosition({boardRight, boardTop});

  float margin = constant::SQUARE_PX_SIZE * 0.2f;
  float bgWidth = m_background.getSize().x;
  auto iconSize = m_flip_icon.getCurrentSize();
  m_flip_icon.setPosition({boardRight + bgWidth / 2.f,
                           boardTop + margin + iconSize.y / 2.f});
  m_icon_highlight.setPosition(m_flip_icon.getPosition());
}

void SettingsBar::render(sf::RenderWindow& window) {
  window.draw(m_background);
  window.draw(m_icon_highlight);
  m_flip_icon.draw(window);
}

bool SettingsBar::isOnFlipIcon(core::MousePos mousePos) const {
  auto pos = m_flip_icon.getPosition();
  auto size = m_flip_icon.getCurrentSize();
  float left = pos.x - size.x / 2.f;
  float right = pos.x + size.x / 2.f;
  float top = pos.y - size.y / 2.f;
  float bottom = pos.y + size.y / 2.f;
  return mousePos.x >= left && mousePos.x <= right && mousePos.y >= top && mousePos.y <= bottom;
}

}  // namespace lilia::view
