#include "lilia/view/move_list_view.hpp"

#include <SFML/Config.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/View.hpp>
#include <algorithm>
#include <sstream>
#include <vector>

#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

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
  if (!m_font.loadFromFile(constant::STR_FILE_PATH_FONT)) {
  }
  // load option icons
  m_icon_resign.setTexture(TextureTable::getInstance().get(constant::STR_FILE_PATH_ICON_RESIGN));
  m_icon_prev.setTexture(TextureTable::getInstance().get(constant::STR_FILE_PATH_ICON_PREV));
  m_icon_next.setTexture(TextureTable::getInstance().get(constant::STR_FILE_PATH_ICON_NEXT));
  m_icon_settings.setTexture(
      TextureTable::getInstance().get(constant::STR_FILE_PATH_ICON_SETTINGS));
  m_icon_new_bot.setTexture(TextureTable::getInstance().get(constant::STR_FILE_PATH_ICON_NEW_BOT));
  m_icon_rematch.setTexture(TextureTable::getInstance().get(constant::STR_FILE_PATH_ICON_REMATCH));
  m_icon_resign.setOriginToCenter();
  m_icon_prev.setOriginToCenter();
  m_icon_next.setOriginToCenter();
  m_icon_settings.setOriginToCenter();
  m_icon_new_bot.setOriginToCenter();
  m_icon_rematch.setOriginToCenter();
}

void MoveListView::setPosition(const Entity::Position &pos) {
  m_position = pos;
}

void MoveListView::setSize(unsigned int width, unsigned int height) {
  m_width = width;
  m_height = height;
  m_option_height = static_cast<float>(m_height) * 0.2f;
  float centerY = static_cast<float>(m_height) - m_option_height / 2.f;
  float padding = 20.f;
  // resign or new bot/rematch on left
  m_icon_resign.setPosition({padding, centerY});
  auto sizeR = m_icon_resign.getCurrentSize();
  m_bounds_resign = {padding - sizeR.x / 2.f, centerY - sizeR.y / 2.f, sizeR.x, sizeR.y};

  m_icon_new_bot.setPosition({padding, centerY});
  auto sizeNB = m_icon_new_bot.getCurrentSize();
  m_bounds_new_bot = {padding - sizeNB.x / 2.f, centerY - sizeNB.y / 2.f, sizeNB.x, sizeNB.y};

  float rematchX = padding + sizeNB.x + 10.f;
  m_icon_rematch.setPosition({rematchX, centerY});
  auto sizeRM = m_icon_rematch.getCurrentSize();
  m_bounds_rematch = {rematchX - sizeRM.x / 2.f, centerY - sizeRM.y / 2.f, sizeRM.x, sizeRM.y};
  // navigation icons in middle
  float midX = static_cast<float>(m_width) / 2.f;
  m_icon_prev.setPosition({midX - 30.f, centerY});
  auto sizeP = m_icon_prev.getCurrentSize();
  m_bounds_prev = {midX - 30.f - sizeP.x / 2.f, centerY - sizeP.y / 2.f, sizeP.x, sizeP.y};
  m_icon_next.setPosition({midX + 30.f, centerY});
  auto sizeN = m_icon_next.getCurrentSize();
  m_bounds_next = {midX + 30.f - sizeN.x / 2.f, centerY - sizeN.y / 2.f, sizeN.x, sizeN.y};
  // settings on right
  m_icon_settings.setPosition({static_cast<float>(m_width) - padding, centerY});
  auto sizeS = m_icon_settings.getCurrentSize();
  m_bounds_settings = {static_cast<float>(m_width) - padding - sizeS.x / 2.f,
                       centerY - sizeS.y / 2.f, sizeS.x, sizeS.y};
}

void MoveListView::setBotMode(bool anyBot) {
  m_any_bot = anyBot;
}

void MoveListView::addMove(const std::string &uciMove) {
  const std::size_t moveIndex = m_move_count;
  const std::size_t lineIndex = moveIndex / 2;
  const bool whiteMove = (moveIndex % 2) == 0;
  const float listHeight = static_cast<float>(m_height) - m_option_height;
  const float contentTop =
      listHeight * kListStartRatio + static_cast<float>(kSubHeaderFontSize) + kMoveSpacing;
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
  const float visibleHeight = listHeight - contentTop;
  const float maxOff = std::max(0.f, content - visibleHeight);
  m_scroll_offset = std::clamp(maxOff, 0.f, maxOff);
}

void MoveListView::addResult(const std::string &result) {
  m_result = result;
  if (m_lines.empty()) {
    m_lines.push_back(result);
  } else {
    m_lines.back() += " " + result;
  }
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

  const float listHeight = static_cast<float>(m_height) - m_option_height;
  const float listTop = listHeight * kListStartRatio;
  const float contentTop = listTop + static_cast<float>(kSubHeaderFontSize) + kMoveSpacing;
  const float top = contentTop;
  const float bottom = listHeight;

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
  sf::RectangleShape movesBg({static_cast<float>(m_width), listHeight - movesBgY});
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

    std::istringstream iss(m_lines[i]);
    std::vector<std::string> tokens;
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    std::string numberStr = tokens.size() > 0 ? tokens[0] : "";
    std::string whiteMove = tokens.size() > 1 ? tokens[1] : "";
    std::string blackMove;
    std::string result;
    if (tokens.size() > 2) {
      if (tokens[2] == "1-0" || tokens[2] == "0-1" || tokens[2] == "1/2-1/2") {
        result = tokens[2];
      } else {
        blackMove = tokens[2];
        if (tokens.size() > 3) result = tokens[3];
      }
    }

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
      x += blackTxt.getLocalBounds().width + kMoveSpacing;
    }

    if (!result.empty()) {
      sf::Text resTxt(result, m_font, kMoveFontSize);
      resTxt.setStyle(sf::Text::Bold);
      resTxt.setFillColor(sf::Color(180, 180, 180));
      resTxt.setPosition(x, y);
      window.draw(resTxt);
    }
  }

  // option field background
  sf::RectangleShape optionBg({static_cast<float>(m_width), m_option_height});
  optionBg.setPosition(0.f, listHeight);
  optionBg.setFillColor(sf::Color(55, 55, 55));
  window.draw(optionBg);
  // draw icons
  if (m_game_over) {
    m_icon_new_bot.draw(window);
    m_icon_rematch.draw(window);
  } else {
    m_icon_resign.draw(window);
  }
  m_icon_prev.draw(window);
  m_icon_next.draw(window);
  m_icon_settings.draw(window);

  window.setView(oldView);
}

void MoveListView::scroll(float delta) {
  m_scroll_offset -= delta * kLineHeight;
  const float listHeight = static_cast<float>(m_height) - m_option_height;
  const float content = static_cast<float>(m_lines.size()) * kLineHeight;
  const float contentTop =
      listHeight * kListStartRatio + static_cast<float>(kSubHeaderFontSize) + kMoveSpacing;
  const float visibleHeight = listHeight - contentTop;
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

  const float listHeight = static_cast<float>(m_height) - m_option_height;
  const float contentTop =
      listHeight * kListStartRatio + static_cast<float>(kSubHeaderFontSize) + kMoveSpacing;
  const float visibleHeight = listHeight - contentTop;

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
  const float listHeight = static_cast<float>(m_height) - m_option_height;
  if (localX < 0.f || localY < 0.f || localX > static_cast<float>(m_width) || localY > listHeight)
    return static_cast<std::size_t>(-1);

  for (std::size_t i = 0; i < m_move_bounds.size(); ++i) {
    if (m_move_bounds[i].contains(localX, localY)) return i;
  }
  return static_cast<std::size_t>(-1);
}

MoveListView::Option MoveListView::getOptionAt(const Entity::Position &pos) const {
  const float localX = pos.x - m_position.x;
  const float localY = pos.y - m_position.y;
  if (m_game_over) {
    if (m_bounds_new_bot.contains(localX, localY)) return Option::NewBot;
    if (m_bounds_rematch.contains(localX, localY)) return Option::Rematch;
  } else {
    if (m_bounds_resign.contains(localX, localY)) return Option::Resign;
  }
  if (m_bounds_prev.contains(localX, localY)) return Option::Prev;
  if (m_bounds_next.contains(localX, localY)) return Option::Next;
  if (m_bounds_settings.contains(localX, localY)) return Option::Settings;
  return Option::None;
}

void MoveListView::setGameOver(bool over) {
  m_game_over = over;
}

}  // namespace lilia::view
