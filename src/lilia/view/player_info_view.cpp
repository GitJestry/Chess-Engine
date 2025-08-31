#include "lilia/view/player_info_view.hpp"

#include <algorithm>  // std::clamp, std::min

#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view {

PlayerInfoView::PlayerInfoView() {
  m_frame.setFillColor(sf::Color::White);
  m_frame.setOutlineColor(sf::Color(100, 100, 100));
  m_frame.setOutlineThickness(2.f);
  m_frame.setSize({32.f, 32.f});
  if (m_font2.loadFromFile(constant::STR_FILE_PATH_FONT)) {
    m_elo.setFont(m_font2);
    m_elo.setCharacterSize(15);
    m_elo.setFillColor({140, 132, 130});
    m_elo.setStyle(sf::Text::Italic);
  }
  if (m_font.loadFromFile(constant::STR_FILE_PATH_FONT)) {
    m_name.setFont(m_font);
    m_name.setCharacterSize(16);
    m_name.setFillColor(sf::Color::White);
    m_name.setStyle(sf::Text::Bold);
  }
}

void PlayerInfoView::setInfo(const PlayerInfo& info) {
  m_icon.setTexture(TextureTable::getInstance().get(info.iconPath));

  // Skaliere so, dass das Icon garantiert in die 32x32-Füllfläche passt (mit kleinem Innenabstand).
  const auto frameSize = m_frame.getSize();  // 32x32
  const float innerPad = 3.f;                // freier Rand innerhalb des Rahmens
  const float targetW = frameSize.x - 2.f * innerPad;
  const float targetH = frameSize.y - 2.f * innerPad;

  auto size = m_icon.getOriginalSize();
  if (size.x > 0 && size.y > 0) {
    const float sx = targetW / static_cast<float>(size.x);
    const float sy = targetH / static_cast<float>(size.y);
    const float scale = std::min(sx, sy) * 1.15;
    m_icon.setScale(scale, scale);
  }

  m_icon.setOriginToCenter();
  if (info.elo == 0) {
    m_name.setString(info.name);
    m_elo.setString("");
  } else {
    m_name.setString(info.name);
    m_elo.setString(" (" + std::to_string(info.elo) + ")");
  }
}

// Variante 1: vorhandene Signatur behalten (kein Clamping möglich ohne Viewport-Größe)
void PlayerInfoView::setPosition(const Entity::Position& pos) {
  m_position = pos;

  // Rahmen setzen
  m_frame.setPosition(pos);

  // Icon exakt auf die Mitte der Füllfläche (Outline wird außen gezeichnet)
  const auto frameSize = m_frame.getSize();  // 32x32
  m_icon.setPosition({pos.x + frameSize.x * 0.5f, pos.y + frameSize.y * 0.5f});

  // Text neben dem Rahmen vertikal zentrieren
  const float textLeft = pos.x + frameSize.x + 12.f;
  const auto tb = m_name.getLocalBounds();  // tb.top ist i.d.R. negativ
  const float textY = pos.y + (frameSize.y - tb.height) * 0.3f - tb.top;
  m_name.setPosition(textLeft, textY);
  m_elo.setPosition(m_name.getPosition().x + tb.width, textY);
}

// Variante 2: Clamping gegen Bildschirmränder
// -> Im Header deklarieren: void setPositionClamped(const Entity::Position&, const sf::Vector2u&
// viewportSize);
void PlayerInfoView::setPositionClamped(const Entity::Position& pos,
                                        const sf::Vector2u& viewportSize) {
  const auto frameSize = m_frame.getSize();             // 32x32 (Füllfläche)
  const float outline = m_frame.getOutlineThickness();  // 2
  const float outerW = frameSize.x + 2.f * outline;     // 36
  const float outerH = frameSize.y + 2.f * outline;     // 36

  const float pad = 8.f;  // Abstand zum Bildschirmrand

  Entity::Position clamped = pos;
  clamped.x = std::clamp(clamped.x, pad, static_cast<float>(viewportSize.x) - outerW - pad);
  clamped.y = std::clamp(clamped.y, pad, static_cast<float>(viewportSize.y) - outerH - pad);

  setPosition(clamped);  // nutzt die Variante oben (zentriert Icon und Text korrekt)
}

void PlayerInfoView::render(sf::RenderWindow& window) {
  window.draw(m_frame);
  m_icon.draw(window);
  window.draw(m_name);
  window.draw(m_elo);
}

}  // namespace lilia::view
