#include "lilia/view/move_list_view.hpp"

#include <SFML/Config.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/View.hpp>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view {

namespace {

// ---------- Layout ----------
constexpr float kPaddingX = 12.f;
constexpr float kPaddingY = 8.f;

constexpr float kRowH = 26.f;     // line height for moves
constexpr float kNumColW = 34.f;  // fixed width for "1." column
constexpr float kMoveGap = 24.f;  // gap between white and black move columns

constexpr float kHeaderH = 54.f;     // top header (title)
constexpr float kSubHeaderH = 28.f;  // "Move List" line
constexpr float kListTopGap = 8.f;   // spacing below subheader before rows

// Fonts
constexpr unsigned kMoveNumberFontSize = 14;
constexpr unsigned kMoveFontSize = 15;
constexpr unsigned kHeaderFontSize = 22;
constexpr unsigned kSubHeaderFontSize = 16;

// ---------- Colors (blends with your start screen) ----------
const sf::Color colSidebarBG(36, 41, 54);  // panel body
const sf::Color colHeaderBG(42, 48, 63);   // header/footer
const sf::Color colListBG(33, 38, 50);     // list background
const sf::Color colRowEven(44, 50, 66);
const sf::Color colRowOdd(38, 44, 58);
const sf::Color colBorder(120, 140, 170, 50);  // hairlines
const sf::Color colAccent(100, 190, 255);      // same accent as start screen

const sf::Color colText(240, 244, 255);
const sf::Color colMuted(180, 186, 205);

// ---------- Helpers ----------
inline float snapf(float v) {
  return std::round(v);
}
inline sf::Vector2f snap(sf::Vector2f p) {
  return {snapf(p.x), snapf(p.y)};
}

// geometry helpers so all calculations stay consistent
inline float listHeight(float totalH, float optionH) {
  return totalH - optionH;
}
inline float contentTop(float totalH, float optionH) {
  (void)totalH;
  (void)optionH;
  return kHeaderH + kSubHeaderH + kListTopGap;
}

}  // namespace

MoveListView::MoveListView() {
  m_font.loadFromFile(constant::STR_FILE_PATH_FONT);

  // load option icons
  m_icon_resign.setTexture(TextureTable::getInstance().get(constant::STR_FILE_PATH_ICON_RESIGN));
  m_icon_resign.setScale(2.f, 2.f);
  m_icon_prev.setTexture(TextureTable::getInstance().get(constant::STR_FILE_PATH_ICON_PREV));
  m_icon_prev.setScale(2.f, 2.f);
  m_icon_next.setTexture(TextureTable::getInstance().get(constant::STR_FILE_PATH_ICON_NEXT));
  m_icon_next.setScale(2.f, 2.f);
  m_icon_settings.setTexture(
      TextureTable::getInstance().get(constant::STR_FILE_PATH_ICON_SETTINGS));
  m_icon_settings.setScale(2.f, 2.f);
  m_icon_new_bot.setTexture(TextureTable::getInstance().get(constant::STR_FILE_PATH_ICON_NEW_BOT));
  m_icon_new_bot.setScale(2.f, 2.f);
  m_icon_rematch.setTexture(TextureTable::getInstance().get(constant::STR_FILE_PATH_ICON_REMATCH));
  m_icon_rematch.setScale(2.f, 2.f);
  m_icon_resign.setOriginToCenter();
  m_icon_prev.setOriginToCenter();
  m_icon_next.setOriginToCenter();
  m_icon_settings.setOriginToCenter();
  m_icon_new_bot.setOriginToCenter();
  m_icon_rematch.setOriginToCenter();
}

void MoveListView::setPosition(const Entity::Position& pos) {
  m_position = pos;
}

void MoveListView::setSize(unsigned int width, unsigned int height) {
  m_width = width;
  m_height = height;

  // footer/options area height
  m_option_height = static_cast<float>(m_height) * 0.20f;
  float listH = listHeight(static_cast<float>(m_height), m_option_height);
  float centerY = listH + (m_option_height / 2.f);
  float pad = 20.f;

  // resign or new bot/rematch on left
  m_icon_resign.setPosition(snap({pad, centerY}));
  auto sizeR = m_icon_resign.getCurrentSize();
  m_bounds_resign = {pad - sizeR.x / 2.f, centerY - sizeR.y / 2.f, sizeR.x, sizeR.y};

  auto sizeNB = m_icon_new_bot.getCurrentSize();
  m_icon_new_bot.setPosition(snap({pad, centerY - sizeNB.y / 2.f}));
  m_bounds_new_bot = {pad - sizeNB.x / 2.f, centerY - sizeNB.y / 2.f, sizeNB.x, sizeNB.y};

  float rematchX = pad;
  m_icon_rematch.setPosition(snap({rematchX, centerY + sizeNB.y / 2.f}));
  auto sizeRM = m_icon_rematch.getCurrentSize();
  m_bounds_rematch = {rematchX - sizeRM.x / 2.f, centerY - sizeRM.y / 2.f, sizeRM.x, sizeRM.y};

  // navigation icons in middle
  float midX = static_cast<float>(m_width) / 2.f;
  m_icon_prev.setPosition(snap({midX - 30.f, centerY}));
  auto sizeP = m_icon_prev.getCurrentSize();
  m_bounds_prev = {midX - 30.f - sizeP.x / 2.f, centerY - sizeP.y / 2.f, sizeP.x, sizeP.y};
  m_icon_next.setPosition(snap({midX + 30.f, centerY}));
  auto sizeN = m_icon_next.getCurrentSize();
  m_bounds_next = {midX + 30.f - sizeN.x / 2.f, centerY - sizeN.y / 2.f, sizeN.x, sizeN.y};

  // settings on right
  m_icon_settings.setPosition(snap({static_cast<float>(m_width) - pad, centerY}));
  auto sizeS = m_icon_settings.getCurrentSize();
  m_bounds_settings = {static_cast<float>(m_width) - pad - sizeS.x / 2.f, centerY - sizeS.y / 2.f,
                       sizeS.x, sizeS.y};
}

void MoveListView::setBotMode(bool anyBot) {
  m_any_bot = anyBot;
}

// --- Add move with fixed column layout (keeps click-targets aligned with rendering) ---
void MoveListView::addMove(const std::string& uciMove) {
  const std::size_t moveIndex = m_move_count;
  const std::size_t lineIndex = moveIndex / 2;
  const bool whiteMoveTurn = (moveIndex % 2) == 0;

  const float listH = listHeight(static_cast<float>(m_height), m_option_height);
  const float y = contentTop(static_cast<float>(m_height), m_option_height) +
                  static_cast<float>(lineIndex) * kRowH;

  if (whiteMoveTurn) {
    const unsigned turn = static_cast<unsigned>(lineIndex + 1);
    std::string numberStr = std::to_string(turn) + ".";
    std::string lineStr = numberStr + " " + uciMove;
    m_lines.push_back(lineStr);

    // click bounds for the white move
    sf::Text wTxt(uciMove, m_font, kMoveFontSize);
    float xWhite = kPaddingX + kNumColW;  // number column is fixed
    float w = wTxt.getLocalBounds().width;
    m_move_bounds.emplace_back(xWhite, y, w, kRowH);
  } else {
    if (!m_lines.empty()) {
      std::string& line = m_lines.back();
      std::size_t spacePos = line.find(' ');
      std::string numberStr = line.substr(0, spacePos);
      std::string whiteStr = (spacePos != std::string::npos) ? line.substr(spacePos + 1) : "";
      line += " " + uciMove;

      sf::Text whiteTxt(whiteStr, m_font, kMoveFontSize);
      float xBlack = kPaddingX + kNumColW + whiteTxt.getLocalBounds().width + kMoveGap;
      sf::Text bTxt(uciMove, m_font, kMoveFontSize);
      float w = bTxt.getLocalBounds().width;
      m_move_bounds.emplace_back(xBlack, y, w, kRowH);
    }
  }

  ++m_move_count;
  m_selected_move = m_move_count ? m_move_count - 1 : m_selected_move;

  // scroll to bottom (respecting visible height)
  const float content = static_cast<float>(m_lines.size() + (m_result.empty() ? 0 : 1)) * kRowH;
  const float topY = contentTop(static_cast<float>(m_height), m_option_height);
  const float visible = listH - topY;
  const float maxOff = std::max(0.f, content - visible);
  m_scroll_offset = maxOff;
}

void MoveListView::addResult(const std::string& result) {
  m_result = result;
  const float listH = listHeight(static_cast<float>(m_height), m_option_height);
  const float topY = contentTop(static_cast<float>(m_height), m_option_height);
  const float visible = listH - topY;
  const float content = static_cast<float>(m_lines.size() + 1) * kRowH;
  const float maxOff = std::max(0.f, content - visible);
  m_scroll_offset = maxOff;
}

void MoveListView::render(sf::RenderWindow& window) const {
  const sf::View oldView = window.getView();

  sf::View view(sf::FloatRect(0.f, 0.f, static_cast<float>(m_width), static_cast<float>(m_height)));
  view.setViewport(
      sf::FloatRect(m_position.x / static_cast<float>(window.getSize().x),
                    m_position.y / static_cast<float>(window.getSize().y),
                    static_cast<float>(m_width) / static_cast<float>(window.getSize().x),
                    static_cast<float>(m_height) / static_cast<float>(window.getSize().y)));
  window.setView(view);

  const float listH = listHeight(static_cast<float>(m_height), m_option_height);
  const float topY = contentTop(static_cast<float>(m_height), m_option_height);

  // --- Background layers (clean, rectangular, matching start screen) ---
  sf::RectangleShape bg({static_cast<float>(m_width), static_cast<float>(m_height)});
  bg.setPosition(0.f, 0.f);
  bg.setFillColor(colSidebarBG);
  window.draw(bg);

  // left hairline to separate from board
  sf::RectangleShape leftLine({1.f, static_cast<float>(m_height)});
  leftLine.setPosition(0.f, 0.f);
  leftLine.setFillColor(colBorder);
  window.draw(leftLine);

  // header
  sf::RectangleShape headerBG({static_cast<float>(m_width), kHeaderH});
  headerBG.setPosition(0.f, 0.f);
  headerBG.setFillColor(colHeaderBG);
  window.draw(headerBG);

  // subheader
  sf::RectangleShape subBG({static_cast<float>(m_width), kSubHeaderH});
  subBG.setPosition(0.f, kHeaderH);
  subBG.setFillColor(colListBG);
  window.draw(subBG);

  // separator lines
  sf::RectangleShape sep({static_cast<float>(m_width), 1.f});
  sep.setFillColor(colBorder);
  sep.setPosition(0.f, kHeaderH);
  window.draw(sep);
  sep.setPosition(0.f, kHeaderH + kSubHeaderH);
  window.draw(sep);
  sep.setPosition(0.f, listH);
  window.draw(sep);

  // list background
  sf::RectangleShape listBG({static_cast<float>(m_width), listH - topY});
  listBG.setPosition(0.f, topY);
  listBG.setFillColor(colListBG);
  window.draw(listBG);

  // --- Titles ---
  sf::Text header(m_any_bot ? "Play Bots" : "2 Player", m_font, kHeaderFontSize);
  header.setStyle(sf::Text::Bold);
  header.setFillColor(colText);
  auto hb = header.getLocalBounds();
  header.setPosition(snapf((m_width - hb.width) / 2.f - hb.left),
                     snapf((kHeaderH - hb.height) / 2.f - hb.top - 2.f));
  window.draw(header);

  sf::Text sub("Move List", m_font, kSubHeaderFontSize);
  sub.setStyle(sf::Text::Bold);
  sub.setFillColor(colMuted);
  auto sb = sub.getLocalBounds();
  sub.setPosition(snapf((m_width - sb.width) / 2.f - sb.left),
                  snapf(kHeaderH + (kSubHeaderH - sb.height) / 2.f - sb.top - 2.f));
  window.draw(sub);

  // --- Alternating rows + selection highlight ---
  const std::size_t totalLines = m_lines.size() + (m_result.empty() ? 0 : 1);
  const float visibleTop = topY;
  const float visibleBottom = listH;

  for (std::size_t i = 0; i < totalLines; ++i) {
    float y = topY + static_cast<float>(i) * kRowH - m_scroll_offset;
    if (y + kRowH < visibleTop || y > visibleBottom) continue;

    sf::RectangleShape row({static_cast<float>(m_width), kRowH});
    row.setPosition(0.f, snapf(y));
    row.setFillColor((i % 2 == 0) ? colRowEven : colRowOdd);
    window.draw(row);
  }

  // Selected move row bar (accent + subtle bar at left)
  if (m_selected_move != static_cast<std::size_t>(-1)) {
    std::size_t rowIdx = m_selected_move / 2;
    float y = topY + static_cast<float>(rowIdx) * kRowH - m_scroll_offset;
    if (y + kRowH >= visibleTop && y <= visibleBottom) {
      // overlay to lift the row just a bit
      sf::RectangleShape hi({static_cast<float>(m_width), kRowH});
      hi.setPosition(0.f, snapf(y));
      hi.setFillColor(sf::Color(80, 100, 120, 40));
      window.draw(hi);

      // left accent bar
      sf::RectangleShape bar({3.f, kRowH});
      bar.setPosition(0.f, snapf(y));
      bar.setFillColor(colAccent);
      window.draw(bar);
    }
  }

  // --- Draw lines (numbers, white, black, result) ---
  for (std::size_t i = 0; i < totalLines; ++i) {
    float y = topY + static_cast<float>(i) * kRowH - m_scroll_offset + 3.f;
    if (y + kRowH < visibleTop || y > visibleBottom) continue;

    // Result-only line
    if (i == m_lines.size() && !m_result.empty()) {
      sf::Text res(m_result, m_font, kMoveFontSize);
      res.setStyle(sf::Text::Bold);
      res.setFillColor(colMuted);
      auto rb = res.getLocalBounds();
      res.setPosition(snapf((m_width - rb.width) / 2.f - rb.left), snapf(y));
      window.draw(res);
      continue;
    }

    // Parse "1. e4 e5" or "1. e4 1-0"
    std::istringstream iss(m_lines[i]);
    std::vector<std::string> toks;
    std::string tok;
    while (iss >> tok) toks.push_back(tok);
    std::string numberStr, whiteMove, blackMove, result;
    if (toks.size() == 1 && (toks[0] == "1-0" || toks[0] == "0-1" || toks[0] == "1/2-1/2")) {
      result = toks[0];
    } else {
      numberStr = toks.size() > 0 ? toks[0] : "";
      whiteMove = toks.size() > 1 ? toks[1] : "";
      if (toks.size() > 2) {
        if (toks[2] == "1-0" || toks[2] == "0-1" || toks[2] == "1/2-1/2") {
          result = toks[2];
        } else {
          blackMove = toks[2];
          if (toks.size() > 3) result = toks[3];
        }
      }
    }

    // number column (fixed width for alignment)
    sf::Text num(numberStr, m_font, kMoveNumberFontSize);
    num.setFillColor(colMuted);
    num.setPosition(snapf(kPaddingX), snapf(y));
    window.draw(num);

    float x = kPaddingX + kNumColW;

    // white move
    sf::Text w(whiteMove, m_font, kMoveFontSize);
    w.setStyle(sf::Text::Bold);
    w.setFillColor((m_selected_move == i * 2) ? colText : colMuted);
    w.setPosition(snapf(x), snapf(y));
    window.draw(w);
    x += w.getLocalBounds().width + kMoveGap;

    // black move (optional)
    if (!blackMove.empty()) {
      sf::Text b(blackMove, m_font, kMoveFontSize);
      b.setStyle(sf::Text::Bold);
      b.setFillColor((m_selected_move == i * 2 + 1) ? colText : colMuted);
      b.setPosition(snapf(x), snapf(y));
      window.draw(b);
      x += b.getLocalBounds().width + kMoveGap;
    }

    // trailing result on the same line (rare)
    if (!result.empty()) {
      sf::Text r(result, m_font, kMoveFontSize);
      r.setStyle(sf::Text::Bold);
      r.setFillColor(colMuted);
      r.setPosition(snapf(x), snapf(y));
      window.draw(r);
    }
  }

  // --- Footer / options tray ---
  sf::RectangleShape optionBG({static_cast<float>(m_width), m_option_height});
  optionBG.setPosition(0.f, listH);
  optionBG.setFillColor(colHeaderBG);
  window.draw(optionBG);

  // top border of footer (already drawn as sep at listH)

  // icons
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
  m_scroll_offset -= delta * kRowH;

  const float listH = listHeight(static_cast<float>(m_height), m_option_height);
  const float topY = contentTop(static_cast<float>(m_height), m_option_height);
  const float visible = listH - topY;
  const float content = static_cast<float>(m_lines.size() + (m_result.empty() ? 0 : 1)) * kRowH;
  const float maxOff = std::max(0.f, content - visible);
  m_scroll_offset = std::clamp(m_scroll_offset, 0.f, maxOff);
}

void MoveListView::clear() {
  m_lines.clear();
  m_move_count = 0;
  m_scroll_offset = 0.f;
  m_selected_move = static_cast<std::size_t>(-1);
  m_move_bounds.clear();
  m_result.clear();
}

void MoveListView::setCurrentMove(std::size_t moveIndex) {
  m_selected_move = moveIndex;
  if (moveIndex == static_cast<std::size_t>(-1)) return;

  const std::size_t lineIndex = moveIndex / 2;
  const float lineY = static_cast<float>(lineIndex) * kRowH;

  const float listH = listHeight(static_cast<float>(m_height), m_option_height);
  const float topY = contentTop(static_cast<float>(m_height), m_option_height);
  const float visible = listH - topY;

  if (lineY < m_scroll_offset) {
    m_scroll_offset = lineY;
  } else if (lineY + kRowH > m_scroll_offset + visible) {
    m_scroll_offset = lineY + kRowH - visible;
  }

  const float content = static_cast<float>(m_lines.size() + (m_result.empty() ? 0 : 1)) * kRowH;
  const float maxOff = std::max(0.f, content - visible);
  m_scroll_offset = std::clamp(m_scroll_offset, 0.f, maxOff);
}

std::size_t MoveListView::getMoveIndexAt(const Entity::Position& pos) const {
  const float localX = pos.x - m_position.x;
  const float localY = pos.y - m_position.y + m_scroll_offset;
  const float listH = listHeight(static_cast<float>(m_height), m_option_height);
  if (localX < 0.f || localY < 0.f || localX > static_cast<float>(m_width) || localY > listH)
    return static_cast<std::size_t>(-1);

  for (std::size_t i = 0; i < m_move_bounds.size(); ++i) {
    if (m_move_bounds[i].contains(localX, localY)) return i;
  }
  return static_cast<std::size_t>(-1);
}

MoveListView::Option MoveListView::getOptionAt(const Entity::Position& pos) const {
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
