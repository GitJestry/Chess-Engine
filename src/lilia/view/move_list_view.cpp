#include "lilia/view/move_list_view.hpp"

#include <SFML/Config.hpp>
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/ConvexShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/Window/Clipboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

#include "lilia/view/render_constants.hpp"

namespace lilia::view {

namespace {

// ---------- Layout ----------
constexpr float kPaddingX = 12.f;

constexpr float kRowH = 26.f;     // line height for moves
constexpr float kNumColW = 56.f;  // wider for 3-digit turns (prevents text collisions)
constexpr float kMoveGap = 30.f;  // gap between white and black move columns

constexpr float kHeaderH = 58.f;     // top header (a bit taller for safety)
constexpr float kFenH = 30.f;        // FEN info row
constexpr float kSubHeaderH = 40.f;  // "Move List" line
constexpr float kListTopGap = 8.f;   // spacing below subheader before rows
constexpr float kFooterH = 54.f;     // fixed footer height (beveled controls)
constexpr float kSlot = 32.f;        // enlarged icon slot size for better visibility
constexpr float kSlotGap = 25.f;     // increased gap for symmetrical spacing
constexpr float kFooterPadX = 25.f;  // horizontal padding inside footer

// Tooltip
constexpr float kTipPadX = 8.f;
constexpr float kTipPadY = 5.f;
constexpr float kTipArrowH = 6.f;

// Toast
constexpr float kToastDur = 1.6f;
constexpr float kToastFade = 0.5f;

// Fonts
constexpr unsigned kMoveNumberFontSize = 14;
constexpr unsigned kMoveFontSize = 15;
constexpr unsigned kHeaderFontSize = 22;
constexpr unsigned kSubHeaderFontSize = 16;
constexpr unsigned kTipFontSize = 13;

// ---------- Colors (theme) ----------

// ---------- Module-local UI state (no header change needed) ----------
static bool g_prevLeftDown = false;
static bool g_toastVisible = false;
static sf::Clock g_toastClock;

// ---------- Helpers ----------
inline float snapf(float v) {
  return std::round(v);
}
inline sf::Vector2f snap(sf::Vector2f p) {
  return {snapf(p.x), snapf(p.y)};
}

inline float listHeight(float totalH, float /*optionH*/) {
  return totalH - kFooterH;
}
inline float contentTop(float /*totalH*/, float /*optionH*/) {
  return kHeaderH + kFenH + kSubHeaderH + kListTopGap;
}

inline sf::Color lighten(sf::Color c, int d) {
  auto clip = [](int x) { return std::clamp(x, 0, 255); };
  return sf::Color(clip(c.r + d), clip(c.g + d), clip(c.b + d), c.a);
}
inline sf::Color darken(sf::Color c, int d) {
  return lighten(c, -d);
}

// Panel soft shadow (inside the viewport)
void drawSoftShadowRect(sf::RenderTarget& t, const sf::FloatRect& r, sf::Color base, int layers = 3,
                        float step = 6.f) {
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

// Thin drop shadow strip under a bar (for separation)
void drawStripShadow(sf::RenderTarget& t, float x, float y, float w) {
  sf::RectangleShape s({w, 2.f});
  s.setPosition(snapf(x), snapf(y));
  s.setFillColor(constant::COL_SHADOW_BAR);
  t.draw(s);
}

// Beveled slot (footer control)
void drawBevelSlot3D(sf::RenderWindow& win, const sf::FloatRect& r, bool hovered) {
  sf::Color base = hovered ? constant::COL_HOVER_BG : constant::COL_SLOT_BASE;

  // body
  sf::RectangleShape body({r.width, r.height});
  body.setPosition(r.left, r.top);
  body.setFillColor(base);
  win.draw(body);

  // bevel
  sf::RectangleShape top({r.width, 1.f});
  top.setPosition(r.left, r.top);
  top.setFillColor(lighten(base, 24));
  win.draw(top);

  sf::RectangleShape bottom({r.width, 1.f});
  bottom.setPosition(r.left, r.top + r.height - 1.f);
  bottom.setFillColor(darken(base, 26));
  win.draw(bottom);

  // border (hairline)
  sf::RectangleShape inset({r.width - 2.f, r.height - 2.f});
  inset.setPosition(r.left + 1.f, r.top + 1.f);
  inset.setFillColor(sf::Color::Transparent);
  inset.setOutlineThickness(1.f);
  inset.setOutlineColor(hovered ? constant::COL_ACCENT_OUTLINE : constant::COL_BORDER_LIGHT);
  win.draw(inset);
}

// slot drawing helpers (icons)
void drawChevron(sf::RenderWindow& win, const sf::FloatRect& slot, bool left, bool hovered) {
  const float s = std::min(slot.width, slot.height) * 0.50f;  // triangle size
  const float x0 = slot.left + (slot.width - s) * 0.5f;
  const float y0 = slot.top + (slot.height - s) * 0.5f;

  sf::ConvexShape tri(3);
  if (left) {
    tri.setPoint(0, {x0 + s, y0});
    tri.setPoint(1, {x0, y0 + s * 0.5f});
    tri.setPoint(2, {x0 + s, y0 + s});
  } else {
    tri.setPoint(0, {x0, y0});
    tri.setPoint(1, {x0 + s, y0 + s * 0.5f});
    tri.setPoint(2, {x0, y0 + s});
  }
  tri.setFillColor(hovered ? constant::COL_ACCENT_HOVER : constant::COL_TEXT);
  win.draw(tri);
}

void drawCrossX(sf::RenderWindow& win, const sf::FloatRect& slot, bool hovered) {
  const float s = std::min(slot.width, slot.height) * 0.70f;
  const float cx = slot.left + slot.width * 0.5f;
  const float cy = slot.top + slot.height * 0.5f;
  const float thick = 2.f;

  sf::RectangleShape bar1({s, thick});
  bar1.setOrigin(s * 0.5f, thick * 0.5f);
  bar1.setPosition(snapf(cx), snapf(cy));
  bar1.setRotation(45.f);
  bar1.setFillColor(hovered ? constant::COL_ACCENT_HOVER : constant::COL_TEXT);

  sf::RectangleShape bar2 = bar1;
  bar2.setRotation(-45.f);

  win.draw(bar1);
  win.draw(bar2);
}

void drawRobot(sf::RenderWindow& win, const sf::FloatRect& slot, bool hovered) {
  const float s = std::min(slot.width, slot.height);
  const float cx = slot.left + slot.width * 0.5f;
  const float cy = slot.top + slot.height * 0.5f;

  sf::RectangleShape head({s * 0.55f, s * 0.42f});
  head.setOrigin(head.getSize() * 0.5f);
  head.setPosition(snapf(cx), snapf(cy + s * 0.04f));
  head.setFillColor(sf::Color::Transparent);
  head.setOutlineThickness(2.f);
  head.setOutlineColor(hovered ? constant::COL_ACCENT_HOVER : constant::COL_TEXT);

  sf::RectangleShape antenna({2.f, s * 0.16f});
  antenna.setOrigin(1.f, antenna.getSize().y);
  antenna.setPosition(snapf(cx), snapf(cy - s * 0.30f));
  antenna.setFillColor(hovered ? constant::COL_ACCENT_HOVER : constant::COL_TEXT);

  sf::RectangleShape eyeL({s * 0.08f, s * 0.10f});
  eyeL.setOrigin(eyeL.getSize() * 0.5f);
  eyeL.setPosition(snapf(cx - s * 0.12f), snapf(cy - s * 0.02f));
  eyeL.setFillColor(hovered ? constant::COL_ACCENT_HOVER : constant::COL_TEXT);

  sf::RectangleShape eyeR = eyeL;
  eyeR.setPosition(snapf(cx + s * 0.12f), snapf(cy - s * 0.02f));

  win.draw(head);
  win.draw(antenna);
  win.draw(eyeL);
  win.draw(eyeR);
}

void drawReload(sf::RenderWindow& win, const sf::FloatRect& slot, bool hovered) {
  // Circular arrow (rematch)
  const float s = std::min(slot.width, slot.height) * 0.70f;
  const float cx = slot.left + slot.width * 0.5f;
  const float cy = slot.top + slot.height * 0.5f;

  sf::CircleShape ring(s * 0.5f);
  ring.setOrigin(s * 0.5f, s * 0.5f);
  ring.setPosition(snapf(cx), snapf(cy));
  ring.setFillColor(sf::Color::Transparent);
  ring.setOutlineThickness(2.f);
  ring.setOutlineColor(hovered ? constant::COL_ACCENT_HOVER : constant::COL_TEXT);
  win.draw(ring);

  // arrow head on top-right
  sf::ConvexShape arrow(3);
  arrow.setPoint(0, {cx + s * 0.12f, cy - s * 0.55f});
  arrow.setPoint(1, {cx + s * 0.42f, cy - s * 0.40f});
  arrow.setPoint(2, {cx + s * 0.15f, cy - s * 0.25f});
  arrow.setFillColor(hovered ? constant::COL_ACCENT_HOVER : constant::COL_TEXT);
  win.draw(arrow);
}

void drawFenIcon(sf::RenderWindow& win, const sf::FloatRect& slot, bool hovered) {
  // simple document icon with folded corner
  const float w = slot.width * 0.8f;
  const float h = slot.height * 0.9f;
  sf::RectangleShape sheet({w, h});
  sheet.setPosition(snapf(slot.left + (slot.width - w) * 0.5f),
                    snapf(slot.top + (slot.height - h) * 0.5f));
  sheet.setFillColor(sf::Color::Transparent);
  sheet.setOutlineThickness(2.f);
  sheet.setOutlineColor(hovered ? constant::COL_ACCENT_HOVER : constant::COL_TEXT);
  win.draw(sheet);

  const float fold = w * 0.25f;
  sf::ConvexShape corner(3);
  corner.setPoint(0, {sheet.getPosition().x + w - fold, sheet.getPosition().y});
  corner.setPoint(1, {sheet.getPosition().x + w, sheet.getPosition().y});
  corner.setPoint(2, {sheet.getPosition().x + w, sheet.getPosition().y + fold});
  corner.setFillColor(hovered ? constant::COL_ACCENT_HOVER : constant::COL_TEXT);
  win.draw(corner);
}

// Tooltip bubble with small down arrow, anchored above a slot center
void drawTooltip(sf::RenderWindow& win, const sf::Vector2f center, const std::string& label,
                 const sf::Font& font) {
  sf::Text t(label, font, kTipFontSize);
  t.setFillColor(constant::COL_TEXT);
  auto b = t.getLocalBounds();

  const float w = b.width + 2.f * kTipPadX;
  const float h = b.height + 2.f * kTipPadY;
  const float x = snapf(center.x - w * 0.5f);
  const float y = snapf(center.y - h - kTipArrowH - 4.f);  // 4px gap from slot

  // shadow
  sf::RectangleShape shadow({w, h});
  shadow.setPosition(x + 2.f, y + 2.f);
  shadow.setFillColor(constant::COL_SHADOW_LIGHT);
  win.draw(shadow);

  // body
  sf::RectangleShape body({w, h});
  body.setPosition(x, y);
  body.setFillColor(constant::COL_TOOLTIP_BG);
  body.setOutlineThickness(1.f);
  body.setOutlineColor(constant::COL_BORDER_LIGHT);
  win.draw(body);

  // arrow
  sf::ConvexShape arrow(3);
  arrow.setPoint(0, {center.x - 6.f, y + h});
  arrow.setPoint(1, {center.x + 6.f, y + h});
  arrow.setPoint(2, {center.x, y + h + kTipArrowH});
  arrow.setFillColor(constant::COL_TOOLTIP_BG);
  win.draw(arrow);

  // text
  t.setPosition(snapf(x + kTipPadX - b.left), snapf(y + kTipPadY - b.top));
  win.draw(t);
}

// Ellipsize long FEN strings keeping tail
std::string ellipsizeRightKeepTail(const std::string& s, sf::Text& probe, float maxW) {
  probe.setString(s);
  if (probe.getLocalBounds().width <= maxW) return s;
  for (std::size_t cut = 0; cut < s.size(); ++cut) {
    std::string view = "..." + s.substr(cut);
    probe.setString(view);
    if (probe.getLocalBounds().width <= maxW) return view;
  }
  return s;
}

}  // namespace

MoveListView::MoveListView() {
  m_font.loadFromFile(constant::STR_FILE_PATH_FONT);
}

void MoveListView::setPosition(const Entity::Position& pos) {
  m_position = pos;
}

void MoveListView::setSize(unsigned int width, unsigned int height) {
  m_width = width;
  m_height = height;

  // footer height is fixed (smaller controls)
  m_option_height = kFooterH;

  const float listH = listHeight(static_cast<float>(m_height), m_option_height);
  const float centerY = listH + (m_option_height * 0.5f);

  // Left: resign OR (new bot + rematch)
  const float left1X = kFooterPadX;
  const float left2X = kFooterPadX + kSlot + kSlotGap;

  // Center: prev / next
  const float midL = (static_cast<float>(m_width) * 0.7f) - (kSlotGap * 0.5f) - kSlot;
  const float midR = (static_cast<float>(m_width) * 0.7f) + (kSlotGap * 0.5f);

  // Assign click bounds (always computed; visibility depends on m_game_over)
  m_bounds_resign = {left1X, centerY - kSlot * 0.5f, kSlot, kSlot};
  m_bounds_new_bot = {left1X, centerY - kSlot * 0.5f, kSlot, kSlot};
  m_bounds_rematch = {left2X, centerY - kSlot * 0.5f, kSlot, kSlot};
  m_bounds_prev = {midL, centerY - kSlot * 0.5f, kSlot, kSlot};
  m_bounds_next = {midR, centerY - kSlot * 0.5f, kSlot, kSlot};

  const float fenIconSize = 18.f;
  m_bounds_fen_icon = {kPaddingX, kHeaderH + (kFenH - fenIconSize) * 0.5f, fenIconSize,
                       fenIconSize};
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

    sf::Text wTxt(uciMove, m_font, kMoveFontSize);
    float xWhite = kPaddingX + kNumColW;  // number column is fixed
    float w = wTxt.getLocalBounds().width;
    m_move_bounds.emplace_back(xWhite, y, w, kRowH);
  } else {
    if (!m_lines.empty()) {
      std::string& line = m_lines.back();
      std::size_t spacePos = line.find(' ');
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

void MoveListView::setFen(const std::string& fen) {
  m_fen_str = fen;
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

  sf::Vector2i mousePx = sf::Mouse::getPosition(window);
  sf::Vector2f mouseLocal = window.mapPixelToCoords(mousePx, view);

  const float listH = listHeight(static_cast<float>(m_height), m_option_height);
  const float topY = contentTop(static_cast<float>(m_height), m_option_height);

  // --- Panel shadow + border (inside our viewport) ---
  {
    const sf::FloatRect panelRect(0.f, 0.f, static_cast<float>(m_width),
                                  static_cast<float>(m_height));
    drawSoftShadowRect(window, panelRect, constant::COL_SHADOW_MEDIUM);
    // outer hairline
    sf::RectangleShape border({panelRect.width + 2.f, panelRect.height + 2.f});
    border.setPosition(snapf(panelRect.left - 1.f), snapf(panelRect.top - 1.f));
    border.setFillColor(constant::COL_BORDER_LIGHT);
    window.draw(border);
  }

  // --- Background layers ---
  sf::RectangleShape bg({static_cast<float>(m_width), static_cast<float>(m_height)});
  bg.setPosition(0.f, 0.f);
  bg.setFillColor(constant::COL_SIDEBAR_BG);
  window.draw(bg);

  // left separator (toward board)
  sf::RectangleShape leftLine({1.f, static_cast<float>(m_height)});
  leftLine.setPosition(0.f, 0.f);
  leftLine.setFillColor(constant::COL_BORDER_LIGHT);
  window.draw(leftLine);

  // header stack
  sf::RectangleShape headerBG({static_cast<float>(m_width), kHeaderH});
  headerBG.setPosition(0.f, 0.f);
  headerBG.setFillColor(constant::COL_HEADER);
  window.draw(headerBG);

  sf::RectangleShape fenBG({static_cast<float>(m_width), kFenH});
  fenBG.setPosition(0.f, kHeaderH);
  fenBG.setFillColor(constant::COL_LIST_BG);
  window.draw(fenBG);

  sf::RectangleShape subBG({static_cast<float>(m_width), kSubHeaderH});
  subBG.setPosition(0.f, kHeaderH + kFenH);
  subBG.setFillColor(constant::COL_LIST_BG);
  window.draw(subBG);

  // hairlines
  sf::RectangleShape sep({static_cast<float>(m_width), 1.f});
  sep.setFillColor(constant::COL_BORDER_LIGHT);
  sep.setPosition(0.f, kHeaderH);
  window.draw(sep);
  sep.setPosition(0.f, kHeaderH + kFenH);
  window.draw(sep);
  sep.setPosition(0.f, kHeaderH + kFenH + kSubHeaderH);
  window.draw(sep);
  sep.setPosition(0.f, listH);
  window.draw(sep);

  // drop shadows under header + FEN rows (prevents perceived overlap)
  drawStripShadow(window, 0.f, kHeaderH - 1.f, static_cast<float>(m_width));
  drawStripShadow(window, 0.f, kHeaderH + kFenH - 1.f, static_cast<float>(m_width));

  // list background
  sf::RectangleShape listBG({static_cast<float>(m_width), listH - topY});
  listBG.setPosition(0.f, topY);
  listBG.setFillColor(constant::COL_LIST_BG);
  window.draw(listBG);

  // --- Titles ---
  sf::Text header(m_any_bot ? "Play Bots" : "2 Player", m_font, kHeaderFontSize);
  header.setStyle(sf::Text::Bold);
  header.setFillColor(constant::COL_TEXT);
  auto hb = header.getLocalBounds();
  header.setPosition(snapf((m_width - hb.width) / 2.f - hb.left),
                     snapf((kHeaderH - hb.height) / 2.f - hb.top - 2.f));
  window.draw(header);

  sf::Text sub("Move List", m_font, kSubHeaderFontSize);
  sub.setStyle(sf::Text::Bold);
  sub.setFillColor(constant::COL_MUTED_TEXT);
  auto sb = sub.getLocalBounds();
  sub.setPosition(snapf((m_width - sb.width) / 2.f - sb.left),
                  snapf(kHeaderH + kFenH + (kSubHeaderH - sb.height) / 2.f - sb.top - 2.f));
  window.draw(sub);

  // FEN line (copy icon + text)
  const bool hovFen = m_bounds_fen_icon.contains(mouseLocal.x, mouseLocal.y);
  drawFenIcon(window, m_bounds_fen_icon, hovFen);
  float textX = m_bounds_fen_icon.left + m_bounds_fen_icon.width + 6.f;
  float availW = static_cast<float>(m_width) - textX - kPaddingX;
  sf::Text probe("", m_font, kMoveFontSize);
  std::string fenDisp = ellipsizeRightKeepTail("FEN: " + m_fen_str, probe, availW);
  sf::Text fenTxt(fenDisp, m_font, kMoveFontSize);
  fenTxt.setFillColor(constant::COL_MUTED_TEXT);
  auto fb = fenTxt.getLocalBounds();
  fenTxt.setPosition(snapf(textX), snapf(kHeaderH + (kFenH - fb.height) / 2.f - fb.top - 2.f));
  window.draw(fenTxt);

  // Clip scrolling content to the list area (map only that band to screen)
  sf::View listView(sf::FloatRect(0.f, topY, static_cast<float>(m_width), listH - topY));
  const auto winSize = window.getSize();
  listView.setViewport(sf::FloatRect(m_position.x / static_cast<float>(winSize.x),
                                     (m_position.y + topY) / static_cast<float>(winSize.y),
                                     static_cast<float>(m_width) / static_cast<float>(winSize.x),
                                     (listH - topY) / static_cast<float>(winSize.y)));
  window.setView(listView);

  // --- Alternating rows + selection highlight ---
  const std::size_t totalLines = m_lines.size() + (m_result.empty() ? 0 : 1);
  const float visibleTop = topY;
  const float visibleBottom = listH;

  for (std::size_t i = 0; i < totalLines; ++i) {
    float y = topY + static_cast<float>(i) * kRowH - m_scroll_offset;
    if (y + kRowH < visibleTop || y > visibleBottom) continue;

    sf::RectangleShape row({static_cast<float>(m_width), kRowH});
    row.setPosition(0.f, snapf(y));
    row.setFillColor((i % 2 == 0) ? constant::COL_ROW_EVEN : constant::COL_ROW_ODD);
    window.draw(row);
  }

  if (m_selected_move != static_cast<std::size_t>(-1)) {
    std::size_t rowIdx = m_selected_move / 2;
    float y = topY + static_cast<float>(rowIdx) * kRowH - m_scroll_offset;
    if (y + kRowH >= visibleTop && y <= visibleBottom) {
      // subtle lift + left accent
      sf::RectangleShape hi({static_cast<float>(m_width), kRowH});
      hi.setPosition(0.f, snapf(y));
      hi.setFillColor(constant::COL_MOVE_HIGHLIGHT);
      window.draw(hi);

      sf::RectangleShape bar({3.f, kRowH});
      bar.setPosition(0.f, snapf(y));
      bar.setFillColor(constant::COL_ACCENT);
      window.draw(bar);
    }
  }

  // --- Draw lines (numbers, white, black, result) ---
  for (std::size_t i = 0; i < totalLines; ++i) {
    float y = topY + static_cast<float>(i) * kRowH - m_scroll_offset + 3.f;
    if (y + kRowH < visibleTop || y > visibleBottom) continue;

    if (i == m_lines.size() && !m_result.empty()) {
      sf::Text res(m_result, m_font, kMoveFontSize);
      res.setStyle(sf::Text::Bold);
      res.setFillColor(constant::COL_MUTED_TEXT);
      auto rb = res.getLocalBounds();
      res.setPosition(snapf((m_width - rb.width) / 2.f - rb.left), snapf(y));
      window.draw(res);
      continue;
    }

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

    sf::Text num(numberStr, m_font, kMoveNumberFontSize);
    num.setFillColor(constant::COL_MUTED_TEXT);
    num.setPosition(snapf(kPaddingX), snapf(y));
    window.draw(num);

    float x = kPaddingX + kNumColW;

    sf::Text w(whiteMove, m_font, kMoveFontSize);
    w.setStyle(sf::Text::Bold);
    w.setFillColor((m_selected_move == i * 2) ? constant::COL_TEXT : constant::COL_MUTED_TEXT);
    w.setPosition(snapf(x), snapf(y));
    window.draw(w);
    x += w.getLocalBounds().width + kMoveGap;

    if (!blackMove.empty()) {
      sf::Text b(blackMove, m_font, kMoveFontSize);
      b.setStyle(sf::Text::Bold);
      b.setFillColor((m_selected_move == i * 2 + 1) ? constant::COL_TEXT : constant::COL_MUTED_TEXT);
      b.setPosition(snapf(x), snapf(y));
      window.draw(b);
      x += b.getLocalBounds().width + kMoveGap;
    }

    if (!result.empty()) {
      sf::Text r(result, m_font, kMoveFontSize);
      r.setStyle(sf::Text::Bold);
      r.setFillColor(constant::COL_MUTED_TEXT);
      r.setPosition(snapf(x), snapf(y));
      window.draw(r);
    }
  }

  window.setView(view);

  // --- Footer / beveled controls ---
  sf::RectangleShape optionBG({static_cast<float>(m_width), m_option_height});
  optionBG.setPosition(0.f, listH);
  optionBG.setFillColor(constant::COL_HEADER);
  window.draw(optionBG);

  // hover detection (in local coords)
  const bool hovPrev = m_bounds_prev.contains(mouseLocal.x, mouseLocal.y);
  const bool hovNext = m_bounds_next.contains(mouseLocal.x, mouseLocal.y);
  const bool hovResign = m_bounds_resign.contains(mouseLocal.x, mouseLocal.y);
  const bool hovNewBot = m_bounds_new_bot.contains(mouseLocal.x, mouseLocal.y);
  const bool hovRematch = m_bounds_rematch.contains(mouseLocal.x, mouseLocal.y);

  // draw slots (3D) + icons
  if (m_game_over) {
    drawBevelSlot3D(window, m_bounds_new_bot, hovNewBot);
    drawRobot(window, m_bounds_new_bot, hovNewBot);

    drawBevelSlot3D(window, m_bounds_rematch, hovRematch);
    drawReload(window, m_bounds_rematch, hovRematch);
  } else {
    drawBevelSlot3D(window, m_bounds_resign, hovResign);
    drawCrossX(window, m_bounds_resign, hovResign);
  }

  drawBevelSlot3D(window, m_bounds_prev, hovPrev);
  drawChevron(window, m_bounds_prev, /*left=*/true, hovPrev);

  drawBevelSlot3D(window, m_bounds_next, hovNext);
  drawChevron(window, m_bounds_next, /*left=*/false, hovNext);

  // --- Tooltips over hovered controls ---
  auto centerOf = [](const sf::FloatRect& r) {
    return sf::Vector2f(r.left + r.width * 0.5f, r.top + r.height * 0.5f);
  };
  if (hovPrev) drawTooltip(window, centerOf(m_bounds_prev), "Previous move", m_font);
  if (hovNext) drawTooltip(window, centerOf(m_bounds_next), "Next move", m_font);
  if (m_game_over) {
    if (hovNewBot) drawTooltip(window, centerOf(m_bounds_new_bot), "New Bot", m_font);
    if (hovRematch) drawTooltip(window, centerOf(m_bounds_rematch), "Rematch", m_font);
  } else {
    if (hovResign) drawTooltip(window, centerOf(m_bounds_resign), "Resign", m_font);
  }
  if (hovFen) {
    // anchor above the icon (slightly right so it doesn't clash with the title)
    auto c = centerOf(m_bounds_fen_icon);
    drawTooltip(window, {c.x + 10.f, c.y}, "Copy FEN", m_font);
  }

  // --- Click-to-copy FEN + toast (handled locally; no header change) ---
  {
    bool leftDown = sf::Mouse::isButtonPressed(sf::Mouse::Left);
    if (leftDown && !g_prevLeftDown && hovFen) {
      // Copy to clipboard and show toast
      sf::Clipboard::setString(m_fen_str);
      g_toastVisible = true;
      g_toastClock.restart();
    }
    g_prevLeftDown = leftDown;
  }

  // --- Toast render ("Copied") inside this view ---
  if (g_toastVisible) {
    float t = g_toastClock.getElapsedTime().asSeconds();
    if (t >= kToastDur) {
      g_toastVisible = false;
    } else {
      float alpha = 1.f;
      if (t > kToastDur - kToastFade) alpha = std::clamp((kToastDur - t) / kToastFade, 0.f, 1.f);

      sf::Text msg("Copied", m_font, 14);
      auto mb = msg.getLocalBounds();
      float padX = 14.f, padY = 8.f;
      sf::Vector2f sz(mb.width + padX * 2.f, mb.height + padY * 2.f);

      // bottom-center of the sidebar panel
      sf::Vector2f pos(snapf((m_width - sz.x) * 0.5f), snapf(m_height - kFooterH - sz.y - 10.f));

      sf::RectangleShape bg({sz.x, sz.y});
      bg.setPosition(pos);
      sf::Color bgCol = constant::COL_TOOLTIP_BG;
      bgCol.a = static_cast<sf::Uint8>(alpha * 220);
      bg.setFillColor(bgCol);
      bg.setOutlineThickness(1.f);
      sf::Color outlineCol = constant::COL_BORDER_LIGHT;
      outlineCol.a = static_cast<sf::Uint8>(alpha * 200);
      bg.setOutlineColor(outlineCol);
      window.draw(bg);

      msg.setFillColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(alpha * 255)));
      msg.setPosition(snapf(pos.x + padX - mb.left), snapf(pos.y + padY - mb.top));
      window.draw(msg);
    }
  }

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
  m_fen_str.clear();
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
  const float localX = pos.x - m_position.x;  // panel-local X
  const float yPanel = pos.y - m_position.y;  // panel-local Y (on screen)
  const float listH = listHeight(static_cast<float>(m_height), m_option_height);
  const float topY = contentTop(static_cast<float>(m_height), m_option_height);

  // Only accept hits inside the visible list band
  if (localX < 0.f || yPanel < topY || localX > static_cast<float>(m_width) || yPanel > listH)
    return static_cast<std::size_t>(-1);

  // Convert screen Y within the band to content-space Y by adding scroll offset
  const float contentY = yPanel + m_scroll_offset;

  for (std::size_t i = 0; i < m_move_bounds.size(); ++i) {
    if (m_move_bounds[i].contains(localX, contentY)) return i;
  }
  return static_cast<std::size_t>(-1);
}

MoveListView::Option MoveListView::getOptionAt(const Entity::Position& pos) const {
  const float localX = pos.x - m_position.x;
  const float localY = pos.y - m_position.y;

  if (m_bounds_fen_icon.contains(localX, localY)) return Option::ShowFen;

  if (m_game_over) {
    if (m_bounds_new_bot.contains(localX, localY)) return Option::NewBot;
    if (m_bounds_rematch.contains(localX, localY)) return Option::Rematch;
  } else {
    if (m_bounds_resign.contains(localX, localY)) return Option::Resign;
  }
  if (m_bounds_prev.contains(localX, localY)) return Option::Prev;
  if (m_bounds_next.contains(localX, localY)) return Option::Next;

  return Option::None;
}

void MoveListView::setGameOver(bool over) {
  m_game_over = over;
}

}  // namespace lilia::view
