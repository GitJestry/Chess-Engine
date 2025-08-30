#include "lilia/view/move_list_view.hpp"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/View.hpp>
#include <algorithm>

#include "lilia/view/render_constants.hpp"

namespace lilia::view {

namespace {
constexpr float kPaddingX = 5.f;
constexpr float kPaddingY = 5.f;
constexpr float kLineHeight = 20.f;
constexpr unsigned kFontSize = 16;
} // namespace

MoveListView::MoveListView() {
  if (!m_font.loadFromFile(
          constant::STR_FILE_PATH_FONT)) { /* Fehlerbehandlung */
  }
}

void MoveListView::setPosition(const Entity::Position &pos) {
  m_position = pos;
}

void MoveListView::setSize(unsigned int width, unsigned int height) {
  m_width = width;
  m_height = height;
}

void MoveListView::addMove(const std::string &uciMove) {
  const std::size_t moveIndex = m_move_count;
  const std::size_t lineIndex = moveIndex / 2;
  const bool whiteMove = (moveIndex % 2) == 0;

  if (whiteMove) {
    const unsigned turn = static_cast<unsigned>(lineIndex + 1);
    std::string prefix = std::to_string(turn) + ". ";
    m_lines.push_back(prefix + uciMove);

    sf::Text pre(prefix, m_font, kFontSize);
    sf::Text moveTxt(uciMove, m_font, kFontSize);
    float x = kPaddingX + pre.getLocalBounds().width;
    float y = kPaddingY + static_cast<float>(lineIndex) * kLineHeight;
    float w = moveTxt.getLocalBounds().width;
    m_move_bounds.emplace_back(x, y, w, kLineHeight);
  } else {
    if (!m_lines.empty()) {
      std::string prefix = m_lines.back();
      sf::Text prefixTxt(prefix, m_font, kFontSize);
      sf::Text space(" ", m_font, kFontSize);
      float x = kPaddingX + prefixTxt.getLocalBounds().width +
                space.getLocalBounds().width;
      float y = kPaddingY + static_cast<float>(lineIndex) * kLineHeight;
      sf::Text moveTxt(uciMove, m_font, kFontSize);
      float w = moveTxt.getLocalBounds().width;
      m_move_bounds.emplace_back(x, y, w, kLineHeight);
      m_lines.back() += " " + uciMove;
    }
  }

  ++m_move_count;
  m_selected_move = m_move_count ? m_move_count - 1 : m_selected_move;

  const float content = static_cast<float>(m_lines.size()) * kLineHeight;
  const float maxOff = std::max(0.f, content - static_cast<float>(m_height));
  m_scroll_offset = std::clamp(maxOff, 0.f, maxOff);
}

void MoveListView::render(sf::RenderWindow &window) const {
  sf::RectangleShape bg;
  bg.setPosition(m_position);
  bg.setSize({static_cast<float>(m_width), static_cast<float>(m_height)});
  bg.setFillColor(sf::Color(30, 30, 30));
  window.draw(bg);

  const sf::View oldView = window.getView();

  sf::View view(sf::FloatRect(0.f, 0.f, static_cast<float>(m_width),
                              static_cast<float>(m_height)));
  view.setViewport(sf::FloatRect(
      m_position.x / static_cast<float>(window.getSize().x),
      m_position.y / static_cast<float>(window.getSize().y),
      static_cast<float>(m_width) / static_cast<float>(window.getSize().x),
      static_cast<float>(m_height) / static_cast<float>(window.getSize().y)));
  window.setView(view);

  const float top = 0.f;
  const float bottom = static_cast<float>(m_height);

  if (m_selected_move != static_cast<std::size_t>(-1) &&
      m_selected_move < m_move_bounds.size()) {
    const auto &rect = m_move_bounds[m_selected_move];
    float y = rect.top - m_scroll_offset;
    if (y + rect.height >= top && y <= bottom) {
      sf::RectangleShape hl({rect.width, rect.height});
      hl.setPosition(rect.left, y);
      hl.setFillColor(sf::Color(80, 80, 80));
      window.draw(hl);
    }
  }

  // Zeichne nur sichtbare Zeilen
  for (std::size_t i = 0; i < m_lines.size(); ++i) {
    const float y =
        kPaddingY + (static_cast<float>(i) * kLineHeight) - m_scroll_offset;
    if (y + kLineHeight < top || y > bottom)
      continue;

    sf::Text text(m_lines[i], m_font, kFontSize);
    text.setFillColor(sf::Color::White);
    text.setPosition(kPaddingX, y);
    window.draw(text);
  }

  window.setView(oldView);
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
  m_selected_move = static_cast<std::size_t>(-1);
  m_move_bounds.clear();
}

void MoveListView::setCurrentMove(std::size_t moveIndex) {
  m_selected_move = moveIndex;
  if (moveIndex == static_cast<std::size_t>(-1))
    return;

  const std::size_t lineIndex = moveIndex / 2;
  const float lineY = lineIndex * kLineHeight;

  if (lineY < m_scroll_offset) {
    m_scroll_offset = lineY;
  } else if (lineY + kLineHeight >
             m_scroll_offset + static_cast<float>(m_height)) {
    m_scroll_offset = lineY + kLineHeight - static_cast<float>(m_height);
  }

  const float content = static_cast<float>(m_lines.size()) * kLineHeight;
  const float maxOff = std::max(0.f, content - static_cast<float>(m_height));
  m_scroll_offset = std::clamp(m_scroll_offset, 0.f, maxOff);
}

std::size_t MoveListView::getMoveIndexAt(const Entity::Position &pos) const {
  const float localX = pos.x - m_position.x;
  const float localY = pos.y - m_position.y + m_scroll_offset;
  if (localX < 0.f || localY < 0.f || localX > static_cast<float>(m_width))
    return static_cast<std::size_t>(-1);

  for (std::size_t i = 0; i < m_move_bounds.size(); ++i) {
    if (m_move_bounds[i].contains(localX, localY))
      return i;
  }
  return static_cast<std::size_t>(-1);
}

} // namespace lilia::view
