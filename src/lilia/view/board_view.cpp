#include "lilia/view/board_view.hpp"

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view {

namespace {
const sf::Color colText(240, 244, 255);
const sf::Color colAccentHover(120, 205, 255);
const sf::Color colDisc(52, 58, 74, 150);       // base disc (translucent)
const sf::Color colDiscHover(60, 68, 86, 180);  // hover disc
const sf::Color colShadow(0, 0, 0, 60);         // micro shadow

inline float snapf(float v) {
  return std::round(v);
}

inline sf::Color lighten(sf::Color c, int d) {
  auto clamp = [](int x) { return std::clamp(x, 0, 255); };
  return sf::Color(clamp(c.r + d), clamp(c.g + d), clamp(c.b + d), c.a);
}
inline sf::Color darken(sf::Color c, int d) {
  return lighten(c, -d);
}

// tiny soft shadow behind a rectangle area
inline void drawSoftShadowRect(sf::RenderTarget &t, const sf::FloatRect &r) {
  const float grow = 2.f;
  sf::RectangleShape s({r.width + 2.f * grow, r.height + 2.f * grow});
  s.setPosition(snapf(r.left - grow), snapf(r.top - grow));
  s.setFillColor(colShadow);
  t.draw(s);
}

void drawFlipIcon(sf::RenderWindow &win, const sf::FloatRect &slot, bool hovered) {
  const float size = std::min(slot.width, slot.height);
  const float cx = slot.left + slot.width * 0.5f;
  const float cy = slot.top + slot.height * 0.5f;

  // ---- Background disc with micro-shadow + bevel ----
  {
    // shadow (very small spread)
    drawSoftShadowRect(win, slot);

    const float R = size * 0.50f;
    sf::CircleShape disc(R);
    disc.setOrigin(R, R);
    disc.setPosition(snapf(cx), snapf(cy));
    disc.setFillColor(hovered ? colDiscHover : colDisc);
    disc.setOutlineThickness(1.f);
    disc.setOutlineColor(hovered ? sf::Color(140, 200, 240, 90) : sf::Color(120, 140, 170, 60));
    win.draw(disc);

    // bevel lines (top highlight + bottom shade)
    sf::CircleShape topHL(R - 1.f);
    topHL.setOrigin(R - 1.f, R - 1.f);
    topHL.setPosition(snapf(cx), snapf(cy));
    topHL.setFillColor(sf::Color::Transparent);
    topHL.setOutlineThickness(1.f);
    topHL.setOutlineColor(lighten(disc.getFillColor(), 16));
    win.draw(topHL);

    sf::CircleShape bottomSH(R - 2.f);
    bottomSH.setOrigin(R - 2.f, R - 2.f);
    bottomSH.setPosition(snapf(cx), snapf(cy));
    bottomSH.setFillColor(sf::Color::Transparent);
    bottomSH.setOutlineThickness(1.f);
    bottomSH.setOutlineColor(darken(disc.getFillColor(), 18));
    win.draw(bottomSH);
  }

  // ---- Twin arrows around the ring (cleaner, centered) ----
  {
    const float ringR = size * 0.38f;
    sf::CircleShape ring(ringR);
    ring.setOrigin(ringR, ringR);
    ring.setPosition(snapf(cx), snapf(cy));
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineThickness(2.f);
    ring.setOutlineColor(hovered ? colAccentHover : colText);
    win.draw(ring);

    const sf::Color ico = hovered ? colAccentHover : colText;
    const float triS = size * 0.22f;

    // upper-right arrow
    sf::ConvexShape a1(3);
    a1.setPoint(0, {cx + triS * 0.00f, cy - triS * 0.65f});
    a1.setPoint(1, {cx + triS * 0.42f, cy - triS * 0.36f});
    a1.setPoint(2, {cx + triS * 0.10f, cy - triS * 0.18f});
    a1.setFillColor(ico);
    win.draw(a1);

    // lower-left arrow
    sf::ConvexShape a2(3);
    a2.setPoint(0, {cx - triS * 0.00f, cy + triS * 0.65f});
    a2.setPoint(1, {cx - triS * 0.42f, cy + triS * 0.36f});
    a2.setPoint(2, {cx - triS * 0.10f, cy + triS * 0.18f});
    a2.setFillColor(ico);
    win.draw(a2);
  }

  // ---- Caption "Flip" (optional; appears just below the icon) ----
  {
    static sf::Font s_font;
    static bool s_loaded = false;
    if (!s_loaded) {
      s_loaded = s_font.loadFromFile(constant::STR_FILE_PATH_FONT);
      if (s_loaded) s_font.setSmooth(false);
    }
    if (s_loaded) {
      sf::Text cap("Flip", s_font, 12);
      cap.setFillColor(hovered ? colAccentHover : colText);
      auto b = cap.getLocalBounds();
      cap.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
      // place just under the slot; tiny lift so it doesn't collide visually
      cap.setPosition(snapf(cx), snapf(slot.top + slot.height + 10.f));
      win.draw(cap);
    }
  }
}
}  // namespace

BoardView::BoardView()
    : m_board({constant::WINDOW_PX_SIZE / 2, constant::WINDOW_PX_SIZE / 2}),
      m_flip_pos(),
      m_flip_size(0.f),
      m_flipped(false) {}

void BoardView::init() {
  m_board.init(TextureTable::getInstance().get(constant::STR_TEXTURE_WHITE),
               TextureTable::getInstance().get(constant::STR_TEXTURE_BLACK),
               TextureTable::getInstance().get(constant::STR_TEXTURE_TRANSPARENT));
  setPosition(getPosition());
}

void BoardView::renderBoard(sf::RenderWindow &window) {
  m_board.draw(window);
  sf::Vector2i mousePx = sf::Mouse::getPosition(window);
  sf::Vector2f mouse = window.mapPixelToCoords(mousePx);
  sf::FloatRect slot(m_flip_pos.x - m_flip_size / 2.f, m_flip_pos.y - m_flip_size / 2.f,
                     m_flip_size, m_flip_size);
  bool hovered = slot.contains(mouse.x, mouse.y);
  drawFlipIcon(window, slot, hovered);
}
[[nodiscard]] Entity::Position BoardView::getSquareScreenPos(core::Square sq) const {
  if (m_flipped) {
    return m_board.getPosOfSquare(
        static_cast<core::Square>(constant::BOARD_SIZE * constant::BOARD_SIZE - 1 - sq));
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

[[nodiscard]] bool BoardView::isFlipped() const {
  return m_flipped;
}

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
  return mousePos.x >= left && mousePos.x <= right && mousePos.y >= top && mousePos.y <= bottom;
}

namespace {
static inline int normalizeUnsignedToSigned(unsigned int u) {
  if (u <= static_cast<unsigned int>(std::numeric_limits<int>::max())) return static_cast<int>(u);
  return -static_cast<int>((std::numeric_limits<unsigned int>::max() - u) + 1u);
}

constexpr int clampInt(int v, int lo, int hi) noexcept {
  return (v < lo) ? lo : (v > hi ? hi : v);
}
}  // namespace

core::MousePos BoardView::clampPosToBoard(core::MousePos mousePos,
                                          Entity::Position pieceSize) const noexcept {
  const int sx = normalizeUnsignedToSigned(mousePos.x);
  const int sy = normalizeUnsignedToSigned(mousePos.y);

  auto boardCenter = getPosition();
  const float halfW = pieceSize.x / 2.f;
  const float halfH = pieceSize.y / 2.f;

  const int left =
      static_cast<int>(boardCenter.x - static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f + halfW);
  const int top =
      static_cast<int>(boardCenter.y - static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f + halfH);
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
  float originX = boardCenter.x - static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f;
  float originY = boardCenter.y - static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f;
  float right = originX + static_cast<float>(constant::WINDOW_PX_SIZE);
  float bottom = originY + static_cast<float>(constant::WINDOW_PX_SIZE);

  if (mousePos.x < originX || mousePos.x >= right || mousePos.y < originY || mousePos.y >= bottom) {
    return core::NO_SQUARE;
  }

  int fileSFML = static_cast<int>((mousePos.x - originX) / constant::SQUARE_PX_SIZE);
  int rankSFML = static_cast<int>((mousePos.y - originY) / constant::SQUARE_PX_SIZE);

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

}  // namespace lilia::view
