#include "lilia/view/move_list_view.hpp"

#include <algorithm>
#include <SFML/Graphics/RectangleShape.hpp>

namespace lilia::view {

MoveListView::MoveListView() {
  if (const sf::Font* def = sf::Font::getDefaultFont()) m_font = *def;
}

void MoveListView::setPosition(const sf::Vector2f& pos) { m_position = pos; }

void MoveListView::setSize(unsigned int width, unsigned int height) {
  m_width = width;
  m_height = height;
}

void MoveListView::addMove(const std::string& uciMove) {
  if (m_move_count % 2 == 0) {
    unsigned int turn = static_cast<unsigned int>(m_move_count / 2 + 1);
    m_lines.push_back(std::to_string(turn) + ". " + uciMove);
  } else {
    if (!m_lines.empty()) m_lines.back() += " " + uciMove;
  }
  ++m_move_count;
}

void MoveListView::render(sf::RenderWindow& window) const {
  sf::RectangleShape bg;
  bg.setPosition(m_position);
  bg.setSize({static_cast<float>(m_width), static_cast<float>(m_height)});
  bg.setFillColor(sf::Color(30, 30, 30));
  window.draw(bg);

  float y = m_position.y - m_scroll_offset + 5.f;
  for (const auto& line : m_lines) {
    sf::Text text(line, m_font, 16);
    text.setFillColor(sf::Color::White);
    text.setPosition(m_position.x + 5.f, y);
    window.draw(text);
    y += 20.f;
  }
}

void MoveListView::scroll(float delta) {
  m_scroll_offset -= delta * 20.f;
  float maxOffset = std::max(0.f, static_cast<float>(m_lines.size()) * 20.f - m_height);
  m_scroll_offset = std::clamp(m_scroll_offset, 0.f, maxOffset);
}

void MoveListView::clear() {
  m_lines.clear();
  m_move_count = 0;
  m_scroll_offset = 0.f;
}

}  // namespace lilia::view

