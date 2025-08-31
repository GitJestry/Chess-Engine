#include "lilia/view/modal_view.hpp"

#include <algorithm>

namespace lilia::view {

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
}

void ModalView::loadFont(const std::string& fontPath) {
  if (m_font.getInfo().family.empty()) {
    if (m_font.loadFromFile(fontPath)) {
      m_font.setSmooth(false);
      m_title.setFont(m_font);
      m_msg.setFont(m_font);
      m_lblLeft.setFont(m_font);
      m_lblRight.setFont(m_font);
      m_lblLeft.setCharacterSize(16);
      m_lblRight.setCharacterSize(16);
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
  auto tb = m_title.getLocalBounds();
  m_title.setPosition(snapf(left + 16.f), snapf(top + 14.f - tb.top));

  // message
  auto mb = m_msg.getLocalBounds();
  m_msg.setPosition(snapf(left + 16.f), snapf(top + 54.f - mb.top));

  // buttons along bottom
  const float by = snapf(top + H - 48.f);
  const float gap = 16.f;

  // left button (x centered left of middle)
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

  // hit rects (slightly bigger than text)
  m_hitLeft = m_btnLeft.getGlobalBounds();
  m_hitRight = m_btnRight.getGlobalBounds();
}

void ModalView::stylePrimaryButton(sf::RectangleShape& btn, sf::Text& lbl) {
  btn.setFillColor(colAccent);
  btn.setOutlineThickness(0.f);
  lbl.setFillColor(sf::Color::Black);
}

void ModalView::styleSecondaryButton(sf::RectangleShape& btn, sf::Text& lbl) {
  btn.setFillColor(colHeader);
  btn.setOutlineThickness(1.f);
  btn.setOutlineColor(colBorder);
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
  stylePrimaryButton(m_btnLeft, m_lblLeft);  // Yes = primary
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
  win.draw(m_border);
  win.draw(m_panel);
  win.draw(m_title);
  win.draw(m_msg);
  win.draw(m_btnLeft);
  win.draw(m_btnRight);
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
