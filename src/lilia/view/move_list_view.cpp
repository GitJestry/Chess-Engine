#include "lilia/view/move_list_view.hpp"

#include <SFML/Graphics/RectangleShape.hpp>
#include <algorithm>

#include "lilia/view/render_constants.hpp"

namespace lilia::view {

namespace {
constexpr float kPaddingX = 5.f;
constexpr float kPaddingY = 5.f;
constexpr float kLineHeight = 20.f;
constexpr unsigned kFontSize = 16;
}  // namespace

MoveListView::MoveListView() {
  if (!m_font.loadFromFile(constant::STR_FILE_PATH_FONT)) { /* Fehlerbehandlung */
  }
}

void MoveListView::setPosition(const Entity::Position& pos) {
  m_position = pos;
}

void MoveListView::setSize(unsigned int width, unsigned int height) {
  m_width = width;
  m_height = height;
}

void MoveListView::addMove(const std::string& uciMove) {
  if ((m_move_count % 2) == 0) {
    const unsigned turn = (m_move_count / 2) + 1;
    m_lines.push_back(std::to_string(turn) + ". " + uciMove);
  } else {
    if (!m_lines.empty()) m_lines.back() += " " + uciMove;
  }
  ++m_move_count;

  // Optional: automatisch nach unten scrollen, wenn neue Zeile dazu kommt
  const float content = static_cast<float>(m_lines.size()) * kLineHeight;
  const float maxOff = std::max(0.f, content - static_cast<float>(m_height));
  m_scroll_offset = std::clamp(maxOff, 0.f, maxOff);
}

void MoveListView::render(sf::RenderWindow& window) const {
  sf::RectangleShape bg;
  bg.setPosition(m_position);
  bg.setSize({static_cast<float>(m_width), static_cast<float>(m_height)});
  bg.setFillColor(sf::Color(30, 30, 30));
  window.draw(bg);

  const float top = m_position.y;
  const float bottom = m_position.y + static_cast<float>(m_height);

  // Zeichne nur sichtbare Zeilen
  for (std::size_t i = 0; i < m_lines.size(); ++i) {
    const float y =
        m_position.y + kPaddingY + (static_cast<float>(i) * kLineHeight) - m_scroll_offset;
    if (y + kLineHeight < top || y > bottom) continue;

    sf::Text text(m_lines[i], m_font, kFontSize);
    text.setFillColor(sf::Color::White);
    text.setPosition(m_position.x + kPaddingX, y);
    window.draw(text);
  }
}

void MoveListView::scroll(float delta) {
  m_scroll_offset -= delta * kLineHeight;
  const float content = static_cast<float>(m_lines.size()) * kLineHeight;
  const float maxOff = std::max(0.f, content - static_cast<float>(m_height));
  m_scroll_offset = std::clamp(m_scroll_offset, 0.f, maxOff);
}

void MoveListView::clear() {
  m_lines.clear();
  m_move_count = 0;
  m_scroll_offset = 0.f;
}

}  // namespace lilia::view
