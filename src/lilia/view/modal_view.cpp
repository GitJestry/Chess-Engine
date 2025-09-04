#include "lilia/view/modal_view.hpp"

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace lilia::view {

namespace {

// --- Pixel snapping for crisp edges/text ---
inline float snapf(float v) {
  return std::round(v);
}
inline sf::Vector2f snap(sf::Vector2f p) {
  return {snapf(p.x), snapf(p.y)};
}

// --- Lighten/Darken helpers (keep alpha) ---
inline sf::Color lighten(sf::Color c, int d) {
  auto clip = [](int x) { return std::clamp(x, 0, 255); };
  return sf::Color(clip(c.r + d), clip(c.g + d), clip(c.b + d), c.a);
}
inline sf::Color darken(sf::Color c, int d) {
  return lighten(c, -d);
}

// --- Soft shadow for rectangular panels (subtle, layered) ---
inline void drawSoftShadowRect(sf::RenderTarget& t, const sf::FloatRect& r, sf::Color base,
                               int layers = 3, float step = 6.f) {
  for (int i = layers; i >= 1; --i) {
    float grow = static_cast<float>(i) * step;
    sf::RectangleShape s({r.width + 2.f * grow, r.height + 2.f * grow});
    s.setPosition(snapf(r.left - grow), snapf(r.top - grow));
    sf::Color c = base;
    c.a = static_cast<sf::Uint8>(std::clamp(30 * i, 0, 255));
    s.setFillColor(c);
    t.draw(s);
  }
}

// --- Beveled 3D button with hover/press states (no rounded corners) ---
inline void drawBevelButton3D(sf::RenderTarget& t, const sf::FloatRect& r, sf::Color base,
                              bool hovered, bool pressed) {
  // Drop shadow
  sf::RectangleShape shadow({r.width, r.height});
  shadow.setPosition(snapf(r.left), snapf(r.top + 2.f));
  shadow.setFillColor(sf::Color(0, 0, 0, 90));
  t.draw(shadow);

  // Body color variations
  sf::Color bodyCol = base;
  if (hovered && !pressed) bodyCol = lighten(bodyCol, 8);
  if (pressed) bodyCol = darken(bodyCol, 6);

  sf::RectangleShape body({r.width, r.height});
  body.setPosition(snapf(r.left), snapf(r.top));
  body.setFillColor(bodyCol);
  t.draw(body);

  // Bevel lines
  sf::RectangleShape topLine({r.width, 1.f});
  topLine.setPosition(snapf(r.left), snapf(r.top));
  topLine.setFillColor(lighten(bodyCol, 24));
  t.draw(topLine);

  sf::RectangleShape bottomLine({r.width, 1.f});
  bottomLine.setPosition(snapf(r.left), snapf(r.top + r.height - 1.f));
  bottomLine.setFillColor(darken(bodyCol, 26));
  t.draw(bottomLine);

  // Thin inset stroke to crisp edges
  sf::RectangleShape inset({r.width - 2.f, r.height - 2.f});
  inset.setPosition(snapf(r.left + 1.f), snapf(r.top + 1.f));
  inset.setFillColor(sf::Color(0, 0, 0, 0));
  inset.setOutlineThickness(1.f);
  inset.setOutlineColor(darken(bodyCol, 18));
  t.draw(inset);
}

// --- Accent inset for selected/primary emphasis ---
inline void drawAccentInset(sf::RenderTarget& t, const sf::FloatRect& r, sf::Color accent) {
  sf::RectangleShape inset({r.width - 2.f, r.height - 2.f});
  inset.setPosition(snapf(r.left + 1.f), snapf(r.top + 1.f));
  inset.setFillColor(sf::Color(0, 0, 0, 0));
  inset.setOutlineThickness(1.f);
  inset.setOutlineColor(accent);
  t.draw(inset);
}

// --- Simple word wrap into the width of `maxBox` using `probe` as metrics ---
inline void wrapTextToWidth(sf::Text& text, const sf::FloatRect& maxBox) {
  std::string s = text.getString();
  std::istringstream iss(s);
  std::string out, word;
  sf::Text probe = text;  // copy for measurement

  float maxW = maxBox.width;
  std::string line;
  while (iss >> word) {
    std::string trial = line.empty() ? word : (line + ' ' + word);
    probe.setString(trial);
    if (probe.getLocalBounds().width > maxW) {
      if (!out.empty()) out.push_back('\n');
      out += line;
      line = word;
    } else {
      line = std::move(trial);
    }
  }
  if (!line.empty()) {
    if (!out.empty()) out.push_back('\n');
    out += line;
  }
  text.setString(out);
}

// --- Vector "X" glyph for the close button (no font dependency) ---
inline void drawCloseGlyph(sf::RenderTarget& t, const sf::FloatRect& r, bool hovered,
                           bool pressed) {
  const float cx = r.left + r.width * 0.5f;
  const float cy = r.top + r.height * 0.5f;
  const float len = std::min(r.width, r.height) * (pressed ? 0.52f : 0.60f);
  const float thick = 2.0f;

  sf::Color col = sf::Color::White;  // accent on hover
  if (pressed) col = darken(col, 24);

  sf::RectangleShape bar({len, thick});
  bar.setOrigin(len * 0.5f, thick * 0.5f);
  bar.setPosition(snapf(cx), snapf(cy));
  bar.setFillColor(col);

  bar.setRotation(45.f);
  t.draw(bar);
  bar.setRotation(-45.f);
  t.draw(bar);
}

}  // namespace

// ------------------ Class impl ------------------

ModalView::ModalView() {
  m_panel.setFillColor(colPanel);
  m_border.setFillColor(colBorder);
  m_overlay.setFillColor(colOverlay);

  m_title.setFillColor(colText);
  m_title.setCharacterSize(20);
  m_title.setStyle(sf::Text::Bold);

  m_msg.setFillColor(colMuted);
  m_msg.setCharacterSize(16);

  m_btnLeft.setSize({120.f, 36.f});
  m_btnRight.setSize({120.f, 36.f});

  // Labels default (fonts attached in loadFont)
  m_lblLeft.setCharacterSize(16);
  m_lblRight.setCharacterSize(16);

  // Close button (vector glyph will be drawn; keep text object unused)
  m_lblClose.setCharacterSize(16);
  m_lblClose.setString("");
}

void ModalView::loadFont(const std::string& fontPath) {
  if (m_font.getInfo().family.empty()) {
    if (m_font.loadFromFile(fontPath)) {
      m_font.setSmooth(false);
      m_title.setFont(m_font);
      m_msg.setFont(m_font);
      m_lblLeft.setFont(m_font);
      m_lblRight.setFont(m_font);
      m_lblClose.setFont(m_font);  // harmless; glyph not used
    }
  }
}

void ModalView::onResize(const sf::Vector2u& windowSize, sf::Vector2f boardCenter) {
  m_windowSize = windowSize;
  m_boardCenter = boardCenter;
  if (m_openResign) layoutCommon({windowSize.x * 0.5f, windowSize.y * 0.5f}, {360.f, 180.f});
  if (m_openGameOver) {
    layoutCommon(boardCenter, {380.f, 190.f});
    layoutGameOverExtras();
  }
}

void ModalView::layoutCommon(sf::Vector2f center, sf::Vector2f panelSize) {
  const float W = panelSize.x, H = panelSize.y;

  // overlay covers the whole window
  m_overlay.setSize({static_cast<float>(m_windowSize.x), static_cast<float>(m_windowSize.y)});
  m_overlay.setPosition(0.f, 0.f);

  // panel & hairline border
  const float left = snapf(center.x - W * 0.5f);
  const float top = snapf(center.y - H * 0.5f);

  m_border.setSize({W + 2.f, H + 2.f});
  m_border.setPosition(left - 1.f, top - 1.f);

  m_panel.setSize({W, H});
  m_panel.setPosition(left, top);

  // title
  m_title.setPosition(snapf(left + 16.f), snapf(top + 16.f - m_title.getLocalBounds().top));

  // message (wrapped inside padded content box)
  const float padX = 16.f;
  const float msgTop = top + 56.f;
  const sf::FloatRect msgBox(left + padX, msgTop, W - 2.f * padX, H - 120.f);
  m_msg.setPosition(snap({msgBox.left, msgBox.top - m_msg.getLocalBounds().top}));
  wrapTextToWidth(m_msg, msgBox);

  // action buttons along bottom
  const float by = snapf(top + H - 52.f);
  const float gap = 16.f;

  float leftBtnX = snapf(left + W * 0.5f - gap - m_btnLeft.getSize().x);
  float rightBtnX = snapf(left + W * 0.5f + gap);

  m_btnLeft.setPosition(leftBtnX, by);
  m_btnRight.setPosition(rightBtnX, by);

  // label centers
  auto lb = m_lblLeft.getLocalBounds();
  m_lblLeft.setPosition(snapf(leftBtnX + (m_btnLeft.getSize().x - lb.width) * 0.5f - lb.left),
                        snapf(by + (m_btnLeft.getSize().y - lb.height) * 0.5f - lb.top));
  auto rb = m_lblRight.getLocalBounds();
  m_lblRight.setPosition(snapf(rightBtnX + (m_btnRight.getSize().x - rb.width) * 0.5f - rb.left),
                         snapf(by + (m_btnRight.getSize().y - rb.height) * 0.5f - rb.top));

  // hit rects
  m_hitLeft = m_btnLeft.getGlobalBounds();
  m_hitRight = m_btnRight.getGlobalBounds();

  // Close button area (square; beveled)
  const float closeSize = 28.f;
  const float cx = left + W - closeSize - 10.f;
  const float cy = top + 10.f;

  m_btnClose.setSize({closeSize, closeSize});
  m_btnClose.setPosition(cx, cy);
  m_btnClose.setFillColor(colHeader);  // matches your header color

  m_hitClose = m_btnClose.getGlobalBounds();
}

void ModalView::layoutGameOverExtras() {
  const float W = m_panel.getSize().x;
  const float left = m_panel.getPosition().x;
  const float top = m_panel.getPosition().y;
  const float centerX = left + W * 0.5f;

  float textTop = top + 40.f;

  if (m_showTrophy) {
    const sf::Color gold(212, 175, 55);
    const float cupW = 60.f;
    const float cupH = 40.f;
    const float stemH = 10.f;
    const float baseH = 10.f;
    const float trophyTop = top + 16.f;

    m_trophyCup.setPointCount(4);
    m_trophyCup.setPoint(0, {0.f, 0.f});
    m_trophyCup.setPoint(1, {cupW, 0.f});
    m_trophyCup.setPoint(2, {cupW * 0.8f, cupH});
    m_trophyCup.setPoint(3, {cupW * 0.2f, cupH});
    m_trophyCup.setFillColor(gold);
    m_trophyCup.setPosition(snapf(centerX - cupW * 0.5f), snapf(trophyTop));

    const float handleR = 12.f;
    m_trophyHandleL.setRadius(handleR);
    m_trophyHandleL.setPointCount(30);
    m_trophyHandleL.setFillColor(sf::Color::Transparent);
    m_trophyHandleL.setOutlineThickness(4.f);
    m_trophyHandleL.setOutlineColor(gold);
    m_trophyHandleL.setPosition(snapf(centerX - cupW * 0.5f - handleR + 1.f),
                                snapf(trophyTop + 5.f));

    m_trophyHandleR.setRadius(handleR);
    m_trophyHandleR.setPointCount(30);
    m_trophyHandleR.setFillColor(sf::Color::Transparent);
    m_trophyHandleR.setOutlineThickness(4.f);
    m_trophyHandleR.setOutlineColor(gold);
    m_trophyHandleR.setPosition(snapf(centerX + cupW * 0.5f - handleR - 1.f),
                                snapf(trophyTop + 5.f));

    m_trophyStem.setSize({cupW * 0.3f, stemH});
    m_trophyStem.setFillColor(gold);
    m_trophyStem.setPosition(snapf(centerX - (cupW * 0.3f) * 0.5f), snapf(trophyTop + cupH));

    m_trophyBase.setSize({cupW * 0.6f, baseH});
    m_trophyBase.setFillColor(gold);
    m_trophyBase.setPosition(snapf(centerX - (cupW * 0.6f) * 0.5f),
                             snapf(trophyTop + cupH + stemH));

    textTop = trophyTop + cupH + stemH + baseH + 12.f;
  }

  auto tb = m_title.getLocalBounds();
  m_title.setPosition(snapf(centerX - tb.width * 0.5f - tb.left), snapf(textTop - tb.top));
}

void ModalView::stylePrimaryButton(sf::RectangleShape& btn, sf::Text& lbl) {
  btn.setFillColor(colAccent);
  btn.setOutlineThickness(0.f);
  lbl.setFillColor(sf::Color::Black);
}

void ModalView::styleSecondaryButton(sf::RectangleShape& btn, sf::Text& lbl) {
  btn.setFillColor(colHeader);
  btn.setOutlineThickness(0.f);
  lbl.setFillColor(colText);
}

// -------- Resign --------
void ModalView::showResign(const sf::Vector2u& ws, sf::Vector2f centerOnBoard) {
  m_windowSize = ws;
  m_openResign = true;
  m_openGameOver = false;
  m_boardCenter = centerOnBoard;

  m_title.setString("Confirm Resign");
  m_msg.setString("Do you really want to resign?");
  m_lblLeft.setString("Yes");
  m_lblRight.setString("No");
  stylePrimaryButton(m_btnLeft, m_lblLeft);      // Yes = primary
  styleSecondaryButton(m_btnRight, m_lblRight);  // No  = secondary

  layoutCommon(centerOnBoard, {360.f, 180.f});
}

void ModalView::hideResign() {
  m_openResign = false;
}
bool ModalView::isResignOpen() const {
  return m_openResign;
}

// -------- Game Over --------
void ModalView::showGameOver(const std::string& msg, bool won, sf::Vector2f centerOnBoard) {
  m_openGameOver = true;
  m_openResign = false;
  m_boardCenter = centerOnBoard;

  m_title.setString(msg);
  m_title.setCharacterSize(28);
  m_title.setStyle(sf::Text::Bold);
  m_msg.setString("");
  m_lblLeft.setString("New Bot");
  m_lblRight.setString("Rematch");
  styleSecondaryButton(m_btnLeft, m_lblLeft);
  stylePrimaryButton(m_btnRight, m_lblRight);

  m_showTrophy = won;

  layoutCommon(centerOnBoard, {380.f, 190.f});
  layoutGameOverExtras();
}

void ModalView::hideGameOver() {
  m_openGameOver = false;
  m_showTrophy = false;
}
bool ModalView::isGameOverOpen() const {
  return m_openGameOver;
}

// -------- Rendering / Hit tests --------
void ModalView::drawOverlay(sf::RenderWindow& win) const {
  if (!(m_openResign || m_openGameOver)) return;
  win.draw(m_overlay);  // NOTE: overlay draws only; we do NOT close on overlay clicks
}

void ModalView::drawPanel(sf::RenderWindow& win) const {
  if (!(m_openResign || m_openGameOver)) return;

  // Soft shadow behind panel
  const sf::FloatRect panelRect = m_panel.getGlobalBounds();
  drawSoftShadowRect(win, panelRect, sf::Color(0, 0, 0, 90));

  // Frame + panel body
  win.draw(m_border);
  win.draw(m_panel);

  // Trophy icon (when a win occurs)
  if (m_openGameOver && m_showTrophy) {
    win.draw(m_trophyHandleL);
    win.draw(m_trophyHandleR);
    win.draw(m_trophyCup);
    win.draw(m_trophyStem);
    win.draw(m_trophyBase);
  }

  // Title + message
  win.draw(m_title);
  win.draw(m_msg);

  // Hover/press state from mouse
  sf::Vector2i mp = sf::Mouse::getPosition(win);
  sf::Vector2f mpos = win.mapPixelToCoords(mp);
  const bool mouseDown = sf::Mouse::isButtonPressed(sf::Mouse::Left);

  const sf::FloatRect leftR = m_btnLeft.getGlobalBounds();
  const sf::FloatRect rightR = m_btnRight.getGlobalBounds();
  const sf::FloatRect closeR = m_btnClose.getGlobalBounds();

  const bool hovLeft = leftR.contains(mpos);
  const bool hovRight = rightR.contains(mpos);
  const bool hovClose = closeR.contains(mpos);

  const bool pressLeft = hovLeft && mouseDown;
  const bool pressRight = hovRight && mouseDown;
  const bool pressClose = hovClose && mouseDown;

  // Beveled buttons with hover/pressed
  drawBevelButton3D(win, leftR, m_btnLeft.getFillColor(), hovLeft, pressLeft);
  drawBevelButton3D(win, rightR, m_btnRight.getFillColor(), hovRight, pressRight);
  drawBevelButton3D(win, closeR, colHeader, hovClose, pressClose);

  // Accent inset on whichever is primary (filled with colAccent)
  if (m_btnLeft.getFillColor() == colAccent) drawAccentInset(win, leftR, sf::Color::White);
  if (m_btnRight.getFillColor() == colAccent) drawAccentInset(win, rightR, sf::Color::White);

  // Labels for the main buttons
  win.draw(m_lblLeft);
  win.draw(m_lblRight);

  // Vector close glyph (replaces font glyph; always renders cleanly)
  drawCloseGlyph(win, closeR, hovClose, pressClose);
}

bool ModalView::hitResignYes(sf::Vector2f p) const {
  return m_openResign && m_hitLeft.contains(p);
}
bool ModalView::hitResignNo(sf::Vector2f p) const {
  return m_openResign && m_hitRight.contains(p);
}
bool ModalView::hitNewBot(sf::Vector2f p) const {
  return m_openGameOver && m_hitLeft.contains(p);
}
bool ModalView::hitRematch(sf::Vector2f p) const {
  return m_openGameOver && m_hitRight.contains(p);
}
bool ModalView::hitClose(sf::Vector2f p) const {
  return (m_openResign || m_openGameOver) && m_hitClose.contains(p);
}

}  // namespace lilia::view
