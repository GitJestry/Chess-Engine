#include "lilia/view/board_view.hpp"

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

#include "lilia/view/color_palette_manager.hpp"
#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view {

namespace {

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

// Soft radial/elliptical shadow under a circle
void drawRadialShadow(sf::RenderTarget& t, sf::Vector2f center, float radius) {
  // Slight vertical squash to read as a drop shadow on the board plane
  const float squash = 0.75f;
  const int layers = 8;
  const float step = 1.8f;    // radius growth per layer
  const float alpha0 = 48.f;  // starting alpha

  for (int i = 0; i < layers; ++i) {
    float R = radius + i * step;
    sf::CircleShape s(R);
    s.setOrigin(R, R);
    s.setPosition(snapf(center.x), snapf(center.y + radius * 0.35f));  // offset downward
    s.setScale(1.f, squash);
    sf::Color c = constant::COL_SHADOW_MEDIUM;
    c.a = static_cast<sf::Uint8>(std::max(0.f, alpha0 * (1.f - i / float(layers))));
    s.setFillColor(c);
    t.draw(s);
  }
}

// Minimal tooltip (same vibe as your sidebar tooltips)
void drawTooltip(sf::RenderWindow& win, const sf::Vector2f center, const std::string& label) {
  static sf::Font s_font;
  static bool s_loaded = false;
  if (!s_loaded) {
    s_loaded = s_font.loadFromFile(constant::STR_FILE_PATH_FONT);
    if (s_loaded) s_font.setSmooth(false);
  }
  if (!s_loaded) return;

  constexpr float padX = 8.f, padY = 5.f, arrowH = 6.f;
  sf::Text t(label, s_font, 12);
  t.setFillColor(constant::COL_TEXT);
  auto b = t.getLocalBounds();
  const float w = b.width + 2.f * padX;
  const float h = b.height + 2.f * padY;
  const float x = snapf(center.x - w * 0.5f);
  const float y = snapf(center.y - h - arrowH - 6.f);

  // shadow
  sf::RectangleShape shadow({w, h});
  shadow.setPosition(x + 2.f, y + 2.f);
  shadow.setFillColor(constant::COL_SHADOW_LIGHT);
  win.draw(shadow);

  // body
  sf::RectangleShape body({w, h});
  body.setPosition(x, y);
  body.setFillColor(constant::COL_TOOLTIP_BG);
  body.setOutlineThickness(1.f);
  body.setOutlineColor(constant::COL_BORDER);
  win.draw(body);

  // arrow
  sf::ConvexShape arrow(3);
  arrow.setPoint(0, {center.x - 6.f, y + h});
  arrow.setPoint(1, {center.x + 6.f, y + h});
  arrow.setPoint(2, {center.x, y + h + arrowH});
  arrow.setFillColor(constant::COL_TOOLTIP_BG);
  win.draw(arrow);

  // text
  t.setPosition(snapf(x + padX - b.left), snapf(y + padY - b.top));
  win.draw(t);
}

// Clean flip icon: disc + ring + two arrowheads
void drawFlipIcon(sf::RenderWindow& win, const sf::FloatRect& slot, bool hovered) {
  const float size = std::min(slot.width, slot.height);
  const float cx = slot.left + slot.width * 0.5f;
  const float cy = slot.top + slot.height * 0.5f;

  // ---- Shadow (radial/elliptical) ----
  drawRadialShadow(win, {cx, cy}, size * 0.48f);

  // ---- Background disc with subtle bevel ----
  const float R = size * 0.50f;
  sf::CircleShape disc(R);
  disc.setOrigin(R, R);
  disc.setPosition(snapf(cx), snapf(cy));
  disc.setFillColor(hovered ? constant::COL_DISC_HOVER : constant::COL_DISC);
  disc.setOutlineThickness(1.f);
  disc.setOutlineColor(hovered ? constant::COL_ACCENT_OUTLINE : constant::COL_BORDER);
  win.draw(disc);

  // inner bevel rings
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

  // ---- Ring + arrows ----
  const float ringR = size * 0.34f;
  sf::CircleShape ring(ringR);
  ring.setOrigin(ringR, ringR);
  ring.setPosition(snapf(cx), snapf(cy));
  ring.setFillColor(sf::Color::Transparent);
  ring.setOutlineThickness(2.f);
  ring.setOutlineColor(hovered ? constant::COL_ACCENT_HOVER : constant::COL_TEXT);
  win.draw(ring);

  const sf::Color ico = hovered ? constant::COL_ACCENT_HOVER : constant::COL_TEXT;
  const float triS = size * 0.22f;

  // Arrowheads placed tangentially to ring
  // upper-right arrow (clockwise)
  {
    const float ax = cx + ringR * 0.85f;
    const float ay = cy - ringR * 0.85f;
    sf::ConvexShape a(3);
    a.setPoint(0, {ax + triS * 0.00f, ay - triS * 0.55f});
    a.setPoint(1, {ax + triS * 0.42f, ay - triS * 0.30f});
    a.setPoint(2, {ax + triS * 0.06f, ay - triS * 0.05f});
    a.setFillColor(ico);
    win.draw(a);
  }
  // lower-left arrow (counter-clockwise)
  {
    const float bx = cx - ringR * 0.85f;
    const float by = cy + ringR * 0.85f;
    sf::ConvexShape a(3);
    a.setPoint(0, {bx - triS * 0.00f, by + triS * 0.55f});
    a.setPoint(1, {bx - triS * 0.42f, by + triS * 0.30f});
    a.setPoint(2, {bx - triS * 0.06f, by + triS * 0.05f});
    a.setFillColor(ico);
    win.draw(a);
  }
}

}  // namespace

BoardView::BoardView()
    : m_board({constant::WINDOW_PX_SIZE / 2, constant::WINDOW_PX_SIZE / 2}),
      m_flip_pos(),
      m_flip_size(0.f),
      m_flipped(false) {
  m_paletteListener = ColorPaletteManager::get().addListener([this]() { onPaletteChanged(); });
}

BoardView::~BoardView() {
  ColorPaletteManager::get().removeListener(m_paletteListener);
}

void BoardView::init() {
  m_board.init(TextureTable::getInstance().get(constant::STR_TEXTURE_WHITE),
               TextureTable::getInstance().get(constant::STR_TEXTURE_BLACK),
               TextureTable::getInstance().get(constant::STR_TEXTURE_TRANSPARENT));
  setPosition(getPosition());
}

void BoardView::onPaletteChanged() {
  m_board.init(TextureTable::getInstance().get(constant::STR_TEXTURE_WHITE),
               TextureTable::getInstance().get(constant::STR_TEXTURE_BLACK),
               TextureTable::getInstance().get(constant::STR_TEXTURE_TRANSPARENT));
  m_board.setFlipped(m_flipped);
  setPosition(getPosition());
}

void BoardView::renderBoard(sf::RenderWindow& window) {
  m_board.draw(window);

  sf::Vector2i mousePx = sf::Mouse::getPosition(window);
  sf::Vector2f mouse = window.mapPixelToCoords(mousePx);

  sf::FloatRect slot(m_flip_pos.x - m_flip_size / 2.f, m_flip_pos.y - m_flip_size / 2.f,
                     m_flip_size, m_flip_size);
  bool hovered = slot.contains(mouse.x, mouse.y);

  drawFlipIcon(window, slot, hovered);

  if (hovered) {
    // Tooltip above icon (consistent with other hovers)
    const float cx = slot.left + slot.width * 0.5f;
    const float cy = slot.top + slot.height * 0.5f;
    drawTooltip(window, {cx, cy}, "Flip board");
  }
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

void BoardView::setPosition(const Entity::Position& pos) {
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
