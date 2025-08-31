#include "lilia/view/board_view.hpp"

#include <limits>

#include <algorithm>
#include <cmath>
#include <SFML/Graphics.hpp>

#include "lilia/view/texture_table.hpp"

namespace lilia::view {

namespace {
const sf::Color colText(240, 244, 255);
const sf::Color colAccentHover(120, 205, 255);

inline float snapf(float v) { return std::round(v); }

void drawFlipIcon(sf::RenderWindow &win, const sf::FloatRect &slot, bool hovered) {
  const float s = std::min(slot.width, slot.height) * 0.70f;
  const float cx = slot.left + slot.width * 0.5f;
  const float cy = slot.top + slot.height * 0.5f;

  sf::CircleShape ring(s * 0.5f);
  ring.setOrigin(s * 0.5f, s * 0.5f);
  ring.setPosition(snapf(cx), snapf(cy));
  ring.setFillColor(sf::Color::Transparent);
  ring.setOutlineThickness(2.f);
  ring.setOutlineColor(hovered ? colAccentHover : colText);
  win.draw(ring);

  sf::ConvexShape arrow1(3);
  arrow1.setPoint(0, {cx + s * 0.12f, cy - s * 0.55f});
  arrow1.setPoint(1, {cx + s * 0.42f, cy - s * 0.40f});
  arrow1.setPoint(2, {cx + s * 0.15f, cy - s * 0.25f});
  arrow1.setFillColor(hovered ? colAccentHover : colText);
  win.draw(arrow1);

  sf::ConvexShape arrow2(3);
  arrow2.setPoint(0, {cx - s * 0.12f, cy + s * 0.55f});
  arrow2.setPoint(1, {cx - s * 0.42f, cy + s * 0.40f});
  arrow2.setPoint(2, {cx - s * 0.15f, cy + s * 0.25f});
  arrow2.setFillColor(hovered ? colAccentHover : colText);
  win.draw(arrow2);
}
}  // namespace

BoardView::BoardView()
    : m_board({constant::WINDOW_PX_SIZE / 2, constant::WINDOW_PX_SIZE / 2}),
      m_flip_pos(), m_flip_size(0.f), m_flipped(false) {}

void BoardView::init() {
  m_board.init(
      TextureTable::getInstance().get(constant::STR_TEXTURE_WHITE),
      TextureTable::getInstance().get(constant::STR_TEXTURE_BLACK),
      TextureTable::getInstance().get(constant::STR_TEXTURE_TRANSPARENT));
  setPosition(getPosition());
}

void BoardView::renderBoard(sf::RenderWindow &window) {
  m_board.draw(window);
  sf::Vector2i mousePx = sf::Mouse::getPosition(window);
  sf::Vector2f mouse = window.mapPixelToCoords(mousePx);
  sf::FloatRect slot(m_flip_pos.x - m_flip_size / 2.f,
                     m_flip_pos.y - m_flip_size / 2.f, m_flip_size, m_flip_size);
  bool hovered = slot.contains(mouse.x, mouse.y);
  drawFlipIcon(window, slot, hovered);
}
[[nodiscard]] Entity::Position
BoardView::getSquareScreenPos(core::Square sq) const {
  if (m_flipped) {
    return m_board.getPosOfSquare(static_cast<core::Square>(
        constant::BOARD_SIZE * constant::BOARD_SIZE - 1 - sq));
  }
  return m_board.getPosOfSquare(sq);
}

void BoardView::toggleFlipped() {
  m_flipped = !m_flipped;
  m_board.setFlipped(m_flipped);
}

void BoardView::setFlipped(bool flipped) {
  m_flipped = flipped;
  m_board.setFlipped(m_flipped);
}

[[nodiscard]] bool BoardView::isFlipped() const { return m_flipped; }

void BoardView::setPosition(const Entity::Position &pos) {
  m_board.setPosition(pos);
  float iconOffset = constant::SQUARE_PX_SIZE * 0.2f;
  m_flip_size = constant::SQUARE_PX_SIZE * 0.3f;
  m_flip_pos = {pos.x + constant::WINDOW_PX_SIZE / 2.f + iconOffset,
                pos.y - constant::WINDOW_PX_SIZE / 2.f + 2.f - iconOffset};
}

[[nodiscard]] Entity::Position BoardView::getPosition() const {
  return m_board.getPosition();
}

[[nodiscard]] bool BoardView::isOnFlipIcon(core::MousePos mousePos) const {
  float left = m_flip_pos.x - m_flip_size / 2.f;
  float right = m_flip_pos.x + m_flip_size / 2.f;
  float top = m_flip_pos.y - m_flip_size / 2.f;
  float bottom = m_flip_pos.y + m_flip_size / 2.f;
  return mousePos.x >= left && mousePos.x <= right && mousePos.y >= top &&
         mousePos.y <= bottom;
}

namespace {
static inline int normalizeUnsignedToSigned(unsigned int u) {
  if (u <= static_cast<unsigned int>(std::numeric_limits<int>::max()))
    return static_cast<int>(u);
  return -static_cast<int>((std::numeric_limits<unsigned int>::max() - u) + 1u);
}

constexpr int clampInt(int v, int lo, int hi) noexcept {
  return (v < lo) ? lo : (v > hi ? hi : v);
}
} // namespace

core::MousePos
BoardView::clampPosToBoard(core::MousePos mousePos,
                            Entity::Position pieceSize) const noexcept {
  const int sx = normalizeUnsignedToSigned(mousePos.x);
  const int sy = normalizeUnsignedToSigned(mousePos.y);

  auto boardCenter = getPosition();
  const float halfW = pieceSize.x / 2.f;
  const float halfH = pieceSize.y / 2.f;

  const int left = static_cast<int>(
      boardCenter.x - static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f + halfW);
  const int top = static_cast<int>(
      boardCenter.y - static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f + halfH);
  const int right = static_cast<int>(
      boardCenter.x + static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f - 1.f - halfW);
  const int bottom = static_cast<int>(
      boardCenter.y + static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f - 1.f - halfH);

  const int cx = clampInt(sx, left, right);
  const int cy = clampInt(sy, top, bottom);

  return {static_cast<unsigned>(cx), static_cast<unsigned>(cy)};
}

core::Square BoardView::mousePosToSquare(core::MousePos mousePos) const {
  auto boardCenter = getPosition();
  float originX =
      boardCenter.x - static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f;
  float originY =
      boardCenter.y - static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f;
  float right = originX + static_cast<float>(constant::WINDOW_PX_SIZE);
  float bottom = originY + static_cast<float>(constant::WINDOW_PX_SIZE);

  if (mousePos.x < originX || mousePos.x >= right || mousePos.y < originY ||
      mousePos.y >= bottom) {
    return core::NO_SQUARE;
  }

  int fileSFML =
      static_cast<int>((mousePos.x - originX) / constant::SQUARE_PX_SIZE);
  int rankSFML =
      static_cast<int>((mousePos.y - originY) / constant::SQUARE_PX_SIZE);

  int fileFromWhite;
  int rankFromWhite;
  if (isFlipped()) {
    fileFromWhite = 7 - fileSFML;
    rankFromWhite = rankSFML;
  } else {
    fileFromWhite = fileSFML;
    rankFromWhite = 7 - rankSFML;
  }

  return static_cast<core::Square>(rankFromWhite * 8 + fileFromWhite);
}

} // namespace lilia::view
