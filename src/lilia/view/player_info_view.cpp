#include "lilia/view/player_info_view.hpp"

#include "lilia/view/render_constants.hpp"

namespace lilia::view {

PlayerInfoView::PlayerInfoView() {
  m_font.loadFromFile(constant::STR_FILE_PATH_FONT);
  m_white_text.setFont(m_font);
  m_black_text.setFont(m_font);
  m_white_text.setCharacterSize(20);
  m_black_text.setCharacterSize(20);
  m_white_text.setFillColor(sf::Color::White);
  m_black_text.setFillColor(sf::Color::White);
}

void PlayerInfoView::setNames(const std::string& white, const std::string& black) {
  m_white_text.setString(white);
  auto wb = m_white_text.getLocalBounds();
  m_white_text.setOrigin(wb.width / 2.f, wb.height / 2.f);

  m_black_text.setString(black);
  auto bb = m_black_text.getLocalBounds();
  m_black_text.setOrigin(bb.width / 2.f, bb.height / 2.f);
}

void PlayerInfoView::layout(float boardCenterX, float boardCenterY, float boardHalf) {
  float offset = 20.f;
  auto wb = m_white_text.getLocalBounds();
  m_white_text.setPosition(
      {boardCenterX, boardCenterY + boardHalf + offset + wb.height / 2.f});
  auto bb = m_black_text.getLocalBounds();
  m_black_text.setPosition(
      {boardCenterX, boardCenterY - boardHalf - offset - bb.height / 2.f});
}

void PlayerInfoView::render(sf::RenderWindow& window) {
  window.draw(m_white_text);
  window.draw(m_black_text);
}

}  // namespace lilia::view

