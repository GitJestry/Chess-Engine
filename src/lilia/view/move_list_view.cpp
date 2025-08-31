#include "lilia/view/move_list_view.hpp"

#include <SFML/Config.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/View.hpp>
#include <algorithm>

#include "lilia/view/render_constants.hpp"

namespace lilia::view {

namespace {
constexpr float kPaddingX = 8.f;
constexpr float kPaddingY = 8.f;
constexpr float kLineHeight = 26.f;
constexpr float kMoveSpacing = 30.f;
constexpr float kListStartRatio = 0.3f;
constexpr unsigned kMoveNumberFontSize = 14;
constexpr unsigned kMoveFontSize = 15;
constexpr unsigned kHeaderFontSize = 24;
constexpr unsigned kSubHeaderFontSize = 18;
}  // namespace

MoveListView::MoveListView() {
  if (!m_font.loadFromFile(constant::STR_FILE_PATH_FONT)) { /* Fehlerbehandlung */
  }
}

void MoveListView::setPosition(const Entity::Position &pos) {
  m_position = pos;
}

void MoveListView::setSize(unsigned int width, unsigned int height) {
  m_width = width;
  m_height = height;
}

void MoveListView::setBotMode(bool anyBot) {
  m_any_bot = anyBot;
}

void MoveListView::addMove(const std::string &uciMove) {
  const std::size_t moveIndex = m_move_count;
  const std::size_t lineIndex = moveIndex / 2;
  const bool whiteMove = (moveIndex % 2) == 0;
  const float contentTop = static_cast<float>(m_height) * kListStartRatio +
                           static_cast<float>(kSubHeaderFontSize) + kMoveSpacing;
  const float y = contentTop + static_cast<float>(lineIndex) * kLineHeight;

  if (whiteMove) {
    const unsigned turn = static_cast<unsigned>(lineIndex + 1);
    std::string numberStr = std::to_string(turn) + ".";
    std::string lineStr = numberStr + " " + uciMove;
    m_lines.push_back(lineStr);

    sf::Text numTxt(numberStr + " ", m_font, kMoveNumberFontSize);
    sf::Text moveTxt(uciMove, m_font, kMoveFontSize);
    float x = kPaddingX + numTxt.getGlobalBounds().width + kMoveSpacing;
    moveTxt.setPosition(x, y);
    float w = moveTxt.getGlobalBounds().width;
    m_move_bounds.emplace_back(x, y, w, kLineHeight);
  } else {
    if (!m_lines.empty()) {
      std::string &line = m_lines.back();
      // parse existing parts
      std::size_t spacePos = line.find(' ');
      std::string numberStr = line.substr(0, spacePos);      // "1."
      std::string whiteMoveStr = line.substr(spacePos + 1);  // "e4"
      line += " " + uciMove;

      sf::Text numTxt(numberStr + " ", m_font, kMoveNumberFontSize);
      sf::Text whiteTxt(whiteMoveStr, m_font, kMoveFontSize);
      sf::Text moveTxt(uciMove, m_font, kMoveFontSize);
      float x = kPaddingX + numTxt.getGlobalBounds().width + kMoveSpacing +
                whiteTxt.getGlobalBounds().width + kMoveSpacing;
      moveTxt.setPosition(x, y);
      float w = moveTxt.getGlobalBounds().width;
      m_move_bounds.emplace_back(x, y, w, kLineHeight);
    }
  }

  ++m_move_count;
  m_selected_move = m_move_count ? m_move_count - 1 : m_selected_move;

  const float content = static_cast<float>(m_lines.size()) * kLineHeight;
  const float visibleHeight = static_cast<float>(m_height) - contentTop;
  const float maxOff = std::max(0.f, content - visibleHeight);
  m_scroll_offset = std::clamp(maxOff, 0.f, maxOff);
}

void MoveListView::render(sf::RenderWindow &window) const {
  const sf::View oldView = window.getView();

  sf::View view(sf::FloatRect(0.f, 0.f, static_cast<float>(m_width), static_cast<float>(m_height)));
  view.setViewport(
      sf::FloatRect(m_position.x / static_cast<float>(window.getSize().x),
                    m_position.y / static_cast<float>(window.getSize().y),
                    static_cast<float>(m_width) / static_cast<float>(window.getSize().x),
                    static_cast<float>(m_height) / static_cast<float>(window.getSize().y)));
  window.setView(view);

  const float listTop = static_cast<float>(m_height) * kListStartRatio;
  const float contentTop = listTop + static_cast<float>(kSubHeaderFontSize) + kMoveSpacing;
  const float top = contentTop;
  const float bottom = static_cast<float>(m_height);

  // Hintergrundsegment neben dem Brett
  sf::RectangleShape segmentBg({static_cast<float>(m_width), static_cast<float>(m_height)});
  segmentBg.setPosition(0.f, 0.f);
  segmentBg.setFillColor(sf::Color(45, 45, 45));
  window.draw(segmentBg);

  // Bereich für Überschriften und Zugliste farblich absetzen
  sf::RectangleShape headerBg(
      {static_cast<float>(m_width), static_cast<float>(kHeaderFontSize) + 2.f * kPaddingY});
  headerBg.setPosition(0.f, 0.f);
  headerBg.setFillColor(sf::Color(55, 55, 55));
  window.draw(headerBg);

  float movesBgY = contentTop;
  sf::RectangleShape movesBg(
      {static_cast<float>(m_width), static_cast<float>(m_height) - movesBgY});
  movesBg.setPosition(0.f, movesBgY);
  movesBg.setFillColor(sf::Color(65, 65, 65));
  window.draw(movesBg);

  // Abwechselnd gefärbte Zeilenhintergründe
  for (std::size_t i = 0; i < m_lines.size(); ++i) {
    float y = contentTop + static_cast<float>(i) * kLineHeight - m_scroll_offset;
    if (y < top || y + kLineHeight > bottom) continue;

    sf::RectangleShape rowBg({static_cast<float>(m_width), kLineHeight});
    const bool even = (i % 2) == 0;
    rowBg.setFillColor(even ? sf::Color(70, 70, 70) : sf::Color(60, 60, 60));
    rowBg.setPosition(0.f, y);
    window.draw(rowBg);
  }

  // Highlight ausgewählten Zug
  if (m_selected_move != static_cast<std::size_t>(-1)) {
    std::size_t lineIndex = m_selected_move / 2;
    float y = contentTop + static_cast<float>(lineIndex) * kLineHeight - m_scroll_offset;
    if (y >= top && y + kLineHeight <= bottom) {
      sf::RectangleShape hl({static_cast<float>(m_width), kLineHeight});
      hl.setPosition(0.f, y);
      hl.setFillColor(sf::Color(90, 90, 90));
      window.draw(hl);
    }
  }

  // Überschriften
  sf::Text header(m_any_bot ? "Play Bots" : "2 Player", m_font, kHeaderFontSize);
  header.setStyle(sf::Text::Bold);
  header.setFillColor(sf::Color::White);
  auto hb = header.getLocalBounds();
  header.setPosition((static_cast<float>(m_width) - hb.width) / 2.f - hb.left, kPaddingY);
  window.draw(header);

  sf::Text subHeader("Movelist", m_font, kSubHeaderFontSize);
  subHeader.setStyle(sf::Text::Bold);
  subHeader.setFillColor(sf::Color::White);
  auto sb = subHeader.getLocalBounds();
  subHeader.setPosition((static_cast<float>(m_width) - sb.width) / 2.f - sb.left, listTop + 10.f);
  window.draw(subHeader);

  // Zeichne nur sichtbare Zeilen
  for (std::size_t i = 0; i < m_lines.size(); ++i) {
    const float y = contentTop + (static_cast<float>(i) * kLineHeight) - m_scroll_offset + 3.f;
    if (y < top || y + kLineHeight > bottom) continue;

    std::string line = m_lines[i];
    std::size_t spacePos = line.find(' ');
    std::string numberStr = line.substr(0, spacePos);
    std::string rest = line.substr(spacePos + 1);
    std::size_t secondSpace = rest.find(' ');
    std::string whiteMove = rest.substr(0, secondSpace);
    std::string blackMove = secondSpace == std::string::npos ? "" : rest.substr(secondSpace + 1);

    sf::Text numTxt(numberStr + " ", m_font, kMoveNumberFontSize);
    numTxt.setStyle(sf::Text::Regular);
    numTxt.setFillColor(sf::Color(180, 180, 180));
    numTxt.setPosition(kPaddingX, y);
    window.draw(numTxt);

    float x = kPaddingX + numTxt.getLocalBounds().width + kMoveSpacing;

    sf::Text whiteTxt(whiteMove, m_font, kMoveFontSize);
    whiteTxt.setStyle(sf::Text::Bold);
    if (m_selected_move == i * 2)
      whiteTxt.setFillColor(sf::Color::White);
    else
      whiteTxt.setFillColor(sf::Color(180, 180, 180));
    whiteTxt.setPosition(x, y);
    window.draw(whiteTxt);
    x += whiteTxt.getLocalBounds().width + kMoveSpacing;

    if (!blackMove.empty()) {
      sf::Text blackTxt(blackMove, m_font, kMoveFontSize);
      blackTxt.setStyle(sf::Text::Bold);
      if (m_selected_move == i * 2 + 1)
        blackTxt.setFillColor(sf::Color::White);
      else
        blackTxt.setFillColor(sf::Color(180, 180, 180));
      blackTxt.setPosition(x, y);
      window.draw(blackTxt);
    }
  }

  window.setView(oldView);
}

void MoveListView::scroll(float delta) {
  m_scroll_offset -= delta * kLineHeight;
  const float content = static_cast<float>(m_lines.size()) * kLineHeight;
  const float contentTop = static_cast<float>(m_height) * kListStartRatio +
                           static_cast<float>(kSubHeaderFontSize) + kMoveSpacing;
  const float visibleHeight = static_cast<float>(m_height) - contentTop;
  const float maxOff = std::max(0.f, content - visibleHeight);
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
  if (moveIndex == static_cast<std::size_t>(-1)) return;

  const std::size_t lineIndex = moveIndex / 2;
  const float lineY = lineIndex * kLineHeight;

  const float contentTop = static_cast<float>(m_height) * kListStartRatio +
                           static_cast<float>(kSubHeaderFontSize) + kMoveSpacing;
  const float visibleHeight = static_cast<float>(m_height) - contentTop;

  if (lineY < m_scroll_offset) {
    m_scroll_offset = lineY;
  } else if (lineY + kLineHeight > m_scroll_offset + visibleHeight) {
    m_scroll_offset = lineY + kLineHeight - visibleHeight;
  }

  const float content = static_cast<float>(m_lines.size()) * kLineHeight;
  const float maxOff = std::max(0.f, content - visibleHeight);
  m_scroll_offset = std::clamp(m_scroll_offset, 0.f, maxOff);
}

std::size_t MoveListView::getMoveIndexAt(const Entity::Position &pos) const {
  const float localX = pos.x - m_position.x;
  const float localY = pos.y - m_position.y + m_scroll_offset;
  if (localX < 0.f || localY < 0.f || localX > static_cast<float>(m_width))
    return static_cast<std::size_t>(-1);

  for (std::size_t i = 0; i < m_move_bounds.size(); ++i) {
    if (m_move_bounds[i].contains(localX, localY)) return i;
  }
  return static_cast<std::size_t>(-1);
}

}  // namespace lilia::view
