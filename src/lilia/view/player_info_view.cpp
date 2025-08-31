#include "lilia/view/player_info_view.hpp"

#include <algorithm>  // std::clamp, std::min
#include <cmath>

#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view {

namespace {
// Palette in sync with the sidebar / start screen
const sf::Color kFrameFill(42, 48, 63);            // #2A303F
const sf::Color kFrameOutline(120, 140, 170, 60);  // subtle hairline
const sf::Color kNameColor(240, 244, 255);         // text
const sf::Color kEloColor(180, 186, 205);          // muted text

inline float snapf(float v) {
  return std::round(v);
}
inline sf::Vector2f snap(sf::Vector2f p) {
  return {snapf(p.x), snapf(p.y)};
}
}  // namespace

PlayerInfoView::PlayerInfoView() {
  // icon frame (32x32 fill area, 1px hairline border)
  m_frame.setFillColor(kFrameFill);
  m_frame.setOutlineColor(kFrameOutline);
  m_frame.setOutlineThickness(1.f);
  m_frame.setSize({32.f, 32.f});

  // font (use the same face for name and ELO)
  if (m_font.loadFromFile(constant::STR_FILE_PATH_FONT)) {
    m_font.setSmooth(false);

    m_name.setFont(m_font);
    m_name.setCharacterSize(16);
    m_name.setFillColor(kNameColor);
    m_name.setStyle(sf::Text::Bold);

    m_elo.setFont(m_font);
    m_elo.setCharacterSize(15);
    m_elo.setFillColor(kEloColor);
    m_elo.setStyle(sf::Text::Regular);  // no italic -> cleaner
  }
}

void PlayerInfoView::setInfo(const PlayerInfo& info) {
  m_icon.setTexture(TextureTable::getInstance().get(info.iconPath));

  // scale icon to fit inside 32x32 frame with a small inner padding
  const auto frameSize = m_frame.getSize();  // 32x32
  const float innerPad = 2.f;
  const float targetW = frameSize.x - 2.f * innerPad;
  const float targetH = frameSize.y - 2.f * innerPad;

  const auto size = m_icon.getOriginalSize();
  if (size.x > 0.f && size.y > 0.f) {
    const float sx = targetW / size.x;
    const float sy = targetH / size.y;
    const float s = std::min(sx, sy);
    m_icon.setScale(s, s);  // exact fit; no overscale
  }
  m_icon.setOriginToCenter();

  // text
  m_name.setString(info.name);
  if (info.elo == 0) {
    m_elo.setString("");
  } else {
    m_elo.setString(" (" + std::to_string(info.elo) + ")");
  }
}

void PlayerInfoView::setPosition(const Entity::Position& pos) {
  m_position = pos;

  // frame
  m_frame.setPosition(snap({pos.x, pos.y}));

  // icon centered in frame (outline is outside the 32x32 fill)
  const auto frameSize = m_frame.getSize();
  m_icon.setPosition(snap({pos.x + frameSize.x * 0.5f, pos.y + frameSize.y * 0.5f}));

  // text baseline aligned and vertically centered to frame
  const float textLeft = pos.x + frameSize.x + 12.f;

  // compute baseline using local bounds (top is usually negative in SFML)
  auto nb = m_name.getLocalBounds();
  const float baselineY = pos.y + (frameSize.y - nb.height) * 0.5f - nb.top;

  m_name.setPosition(snap({textLeft, baselineY}));

  // place ELO right after name with a small gap
  auto nameBounds = m_name.getLocalBounds();
  const float eloX = textLeft + nameBounds.width + 6.f;
  m_elo.setPosition(snap({eloX, baselineY}));

  layoutCaptured();
}

void PlayerInfoView::setPositionClamped(const Entity::Position& pos,
                                        const sf::Vector2u& viewportSize) {
  const auto frameSize = m_frame.getSize();             // 32x32
  const float outline = m_frame.getOutlineThickness();  // 1
  const float outerW = frameSize.x + 2.f * outline;
  const float outerH = frameSize.y + 2.f * outline;

  const float pad = 8.f;  // screen edge padding

  Entity::Position clamped = pos;
  clamped.x = std::clamp(clamped.x, pad, static_cast<float>(viewportSize.x) - outerW - pad);
  clamped.y = std::clamp(clamped.y, pad, static_cast<float>(viewportSize.y) - outerH - pad);

  setPosition(clamped);
}

void PlayerInfoView::render(sf::RenderWindow& window) {
  window.draw(m_frame);
  m_icon.draw(window);
  window.draw(m_name);
  window.draw(m_elo);
  for (auto& piece : m_capturedPieces) {
    piece.draw(window);
  }
}

void PlayerInfoView::addCapturedPiece(core::PieceType type, core::Color color) {
  std::uint8_t numTypes = 6;
  std::string filename = constant::ASSET_PIECES_FILE_PATH + "/piece_" +
                         std::to_string(static_cast<std::uint8_t>(type) +
                                        numTypes * static_cast<std::uint8_t>(color)) +
                         ".png";
  const sf::Texture& texture = TextureTable::getInstance().get(filename);
  Entity piece(texture);
  piece.setScale(constant::ASSET_PIECE_SCALE * 0.6f,
                 constant::ASSET_PIECE_SCALE * 0.6f);
  m_capturedPieces.push_back(std::move(piece));
  layoutCaptured();
}

void PlayerInfoView::removeCapturedPiece() {
  if (!m_capturedPieces.empty()) {
    m_capturedPieces.pop_back();
    layoutCaptured();
  }
}

void PlayerInfoView::clearCapturedPieces() {
  m_capturedPieces.clear();
}

void PlayerInfoView::layoutCaptured() {
  float x = m_position.x;
  float y = m_position.y + m_frame.getSize().y + 6.f;
  const float gap = 4.f;
  for (auto& piece : m_capturedPieces) {
    piece.setPosition(snap({x, y}));
    x += piece.getCurrentSize().x + gap;
  }
}

}  // namespace lilia::view
