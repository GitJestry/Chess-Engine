#include "lilia/view/modal_view.hpp"

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

// --- Soft shadow for rectangular panels ---
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

// --- Beveled button (no rounded corners) ---
inline void drawBevelButton3D(sf::RenderTarget& t, const sf::FloatRect& r, sf::Color base) {
  // subtle drop shadow
  sf::RectangleShape shadow({r.width, r.height});
  shadow.setPosition(snapf(r.left), snapf(r.top + 2.f));
  shadow.setFillColor(sf::Color(0, 0, 0, 90));
  t.draw(shadow);

  // body
  sf::RectangleShape body({r.width, r.height});
  body.setPosition(snapf(r.left), snapf(r.top));
  body.setFillColor(base);
  t.draw(body);

  // bevel lines
  sf::RectangleShape topLine({r.width, 1.f});
  topLine.setPosition(snapf(r.left), snapf(r.top));
  topLine.setFillColor(lighten(base, 24));
  t.draw(topLine);

  sf::RectangleShape bottomLine({r.width, 1.f});
  bottomLine.setPosition(snapf(r.left), snapf(r.top + r.height - 1.f));
  bottomLine.setFillColor(darken(base, 26));
  t.draw(bottomLine);

  // thin inset stroke to crisp edges
  sf::RectangleShape inset({r.width - 2.f, r.height - 2.f});
  inset.setPosition(snapf(r.left + 1.f), snapf(r.top + 1.f));
  inset.setFillColor(sf::Color(0, 0, 0, 0));
  inset.setOutlineThickness(1.f);
  inset.setOutlineColor(darken(base, 18));
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

}  // namespace

// ------------------ Class impl ------------------

ModalView::ModalView() {
  // Base colors come from theme constants declared in the header (colPanel, colBorder, colOverlay,
  // ...)
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

  // Labels default
  m_lblLeft.setCharacterSize(16);
  m_lblRight.setCharacterSize(16);
}

void ModalView::loadFont(const std::string& fontPath) {
  if (m_font.getInfo().family.empty()) {
    if (m_font.loadFromFile(fontPath)) {
      m_font.setSmooth(false);
      m_title.setFont(m_font);
      m_msg.setFont(m_font);
      m_lblLeft.setFont(m_font);
      m_lblRight.setFont(m_font);
    }
  }
}

void ModalView::onResize(const sf::Vector2u& windowSize, sf::Vector2f boardCenter) {
  m_windowSize = windowSize;
  m_boardCenter = boardCenter;
  // keep panels centered on their respective anchors
  if (m_openResign) layoutCommon({windowSize.x * 0.5f, windowSize.y * 0.5f}, {360.f, 180.f});
  if (m_openGameOver) layoutCommon(boardCenter, {380.f, 190.f});
}

void ModalView::layoutCommon(sf::Vector2f center, sf::Vector2f panelSize) {
  const float W = panelSize.x, H = panelSize.y;

  // overlay covers the whole window
  m_overlay.setSize({static_cast<float>(m_windowSize.x), static_cast<float>(m_windowSize.y)});
  m_overlay.setPosition(0.f, 0.f);

  // panel & border (hairline)
  const float left = snapf(center.x - W * 0.5f);
  const float top = snapf(center.y - H * 0.5f);

  m_border.setSize({W + 2.f, H + 2.f});
  m_border.setPosition(left - 1.f, top - 1.f);

  m_panel.setSize({W, H});
  m_panel.setPosition(left, top);

  // title
  m_title.setPosition(snapf(left + 16.f), snapf(top + 16.f - m_title.getLocalBounds().top));

  // message (wrap to inner width)
  const float padX = 16.f;
  const float msgTop = top + 56.f;
  const sf::FloatRect msgBox(left + padX, msgTop, W - 2.f * padX, H - 120.f);
  m_msg.setPosition(snap({msgBox.left, msgBox.top - m_msg.getLocalBounds().top}));
  wrapTextToWidth(m_msg, msgBox);  // reflow based on current string & font

  // buttons along bottom
  const float by = snapf(top + H - 52.f);
  const float gap = 16.f;

  // left & right buttons symmetrically around center
  float leftBtnX = snapf(left + W * 0.5f - gap - m_btnLeft.getSize().x);
  float rightBtnX = snapf(left + W * 0.5f + gap);

  m_btnLeft.setPosition(leftBtnX, by);
  m_btnRight.setPosition(rightBtnX, by);

  // center labels inside buttons
  auto lb = m_lblLeft.getLocalBounds();
  m_lblLeft.setPosition(snapf(leftBtnX + (m_btnLeft.getSize().x - lb.width) * 0.5f - lb.left),
                        snapf(by + (m_btnLeft.getSize().y - lb.height) * 0.5f - lb.top));

  auto rb = m_lblRight.getLocalBounds();
  m_lblRight.setPosition(snapf(rightBtnX + (m_btnRight.getSize().x - rb.width) * 0.5f - rb.left),
                         snapf(by + (m_btnRight.getSize().y - rb.height) * 0.5f - rb.top));

  // hit rects (match visual buttons)
  m_hitLeft = m_btnLeft.getGlobalBounds();
  m_hitRight = m_btnRight.getGlobalBounds();
}

void ModalView::stylePrimaryButton(sf::RectangleShape& btn, sf::Text& lbl) {
  // Keep for compatibility; actual draw uses bevel with this color as base.
  btn.setFillColor(colAccent);
  btn.setOutlineThickness(0.f);
  lbl.setFillColor(sf::Color::Black);
}

void ModalView::styleSecondaryButton(sf::RectangleShape& btn, sf::Text& lbl) {
  // Use header color (darker panel) as base for bevel.
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
  stylePrimaryButton(m_btnLeft, m_lblLeft);  // Yes = primary (accent)
  styleSecondaryButton(m_btnRight, m_lblRight);

  layoutCommon(centerOnBoard, {360.f, 180.f});
}

void ModalView::hideResign() {
  m_openResign = false;
}
bool ModalView::isResignOpen() const {
  return m_openResign;
}

// -------- Game Over --------
void ModalView::showGameOver(const std::string& msg, sf::Vector2f centerOnBoard) {
  m_openGameOver = true;
  m_openResign = false;
  m_boardCenter = centerOnBoard;

  m_title.setString("Game Over");
  m_msg.setString(msg);
  m_lblLeft.setString("New Bot");
  m_lblRight.setString("Rematch");
  styleSecondaryButton(m_btnLeft, m_lblLeft);
  stylePrimaryButton(m_btnRight, m_lblRight);  // Rematch = primary

  layoutCommon(centerOnBoard, {380.f, 190.f});
}

void ModalView::hideGameOver() {
  m_openGameOver = false;
}
bool ModalView::isGameOverOpen() const {
  return m_openGameOver;
}

// -------- Rendering / Hit tests --------
void ModalView::drawOverlay(sf::RenderWindow& win) const {
  if (!(m_openResign || m_openGameOver)) return;
  win.draw(m_overlay);
}

void ModalView::drawPanel(sf::RenderWindow& win) const {
  if (!(m_openResign || m_openGameOver)) return;

  // Panel soft shadow + border + body
  const sf::FloatRect panelRect = m_panel.getGlobalBounds();
  drawSoftShadowRect(win, panelRect, sf::Color(0, 0, 0, 90));

  // hairline border (existing)
  win.draw(m_border);
  // body
  win.draw(m_panel);

  // Title + message
  win.draw(m_title);
  win.draw(m_msg);

  // Buttons: draw beveled using each button’s global bounds; then labels
  const sf::FloatRect leftR = m_btnLeft.getGlobalBounds();
  const sf::FloatRect rightR = m_btnRight.getGlobalBounds();

  // Use each button's fill as base color (set by stylePrimary/Secondary)
  drawBevelButton3D(win, leftR, m_btnLeft.getFillColor());
  drawBevelButton3D(win, rightR, m_btnRight.getFillColor());

  // Accent inset on the primary button (accent == primary color)
  if (m_btnLeft.getFillColor() == colAccent) drawAccentInset(win, leftR, sf::Color::White);
  if (m_btnRight.getFillColor() == colAccent) drawAccentInset(win, rightR, sf::Color::White);

  // Labels
  win.draw(m_lblLeft);
  win.draw(m_lblRight);
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

}  // namespace lilia::view
