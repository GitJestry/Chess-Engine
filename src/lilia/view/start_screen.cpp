#include "lilia/view/start_screen.hpp"

#include <SFML/Graphics.hpp>
#include <algorithm>  // for std::clamp, std::max
#include <cctype>
#include <cmath>
#include <sstream>
#include <unordered_map>

#include "lilia/bot/bot_info.hpp"
#include "lilia/view/render_constants.hpp"

namespace lilia::view {

// ---------------------- helpers ----------------------

namespace {
constexpr float PANEL_W = 820.f;
constexpr float PANEL_H = 520.f;

constexpr float BTN_H = 44.f;
constexpr float BTN_W = 180.f;

constexpr float LIST_ITEM_H = 36.f;
constexpr float SHADOW_OFFSET = 6.f;

constexpr float POPUP_W = 700.f;
constexpr float POPUP_H = 240.f;

constexpr float ERROR_FADE_SEC = 1.6f;

// --- Time panel (smaller + centered) ---
constexpr float TIME_W = 200.f;  // ~half the previous width
constexpr float TIME_H = 120.f;  // compact height
constexpr float CHIP_H = 24.f;
constexpr float CHIP_GAP = 10.f;

sf::Color colBGTop(24, 29, 38);
sf::Color colBGBottom(16, 19, 26);

// Glass panel + border (rectangular)
sf::Color colPanel(36, 41, 54, 150);
sf::Color colTextPanel(36, 41, 54);
sf::Color colPanelBorder(180, 186, 205, 50);
sf::Color colShadow(0, 0, 0, 90);

sf::Color colButton(58, 64, 80);
sf::Color colButtonHover(72, 78, 96);
sf::Color colButtonActive(92, 98, 120);
sf::Color colAccent(100, 190, 255);
sf::Color colText(240, 244, 255);
sf::Color colSubtle(180, 186, 205);

sf::Color colInput(44, 50, 66);
sf::Color colInputBorder(120, 140, 180);

// --- pixel snapping ---
inline float snapf(float v) {
  return std::round(v);
}
inline sf::Vector2f snap(sf::Vector2f v) {
  return {snapf(v.x), snapf(v.y)};
}

inline void centerText(sf::Text& t, const sf::FloatRect& box, float dy = 0.f) {
  auto b = t.getLocalBounds();
  t.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
  t.setPosition(snapf(box.left + box.width / 2.f), snapf(box.top + box.height / 2.f + dy));
}
inline void leftCenterText(sf::Text& t, const sf::FloatRect& box, float padX, float dy = 0.f) {
  auto b = t.getLocalBounds();
  t.setOrigin(b.left, b.top + b.height / 2.f);
  t.setPosition(snapf(box.left + padX), snapf(box.top + box.height / 2.f + dy));
}

void drawVerticalGradient(sf::RenderWindow& window, sf::Color top, sf::Color bottom) {
  sf::VertexArray va(sf::TriangleStrip, 4);
  auto size = window.getSize();
  va[0].position = {0.f, 0.f};
  va[1].position = {static_cast<float>(size.x), 0.f};
  va[2].position = {0.f, static_cast<float>(size.y)};
  va[3].position = {static_cast<float>(size.x), static_cast<float>(size.y)};
  va[0].color = va[1].color = top;
  va[2].color = va[3].color = bottom;
  window.draw(va);
}

// rectangular soft shadow
inline void drawSoftShadowRect(sf::RenderTarget& t, const sf::FloatRect& r) {
  for (int i = 3; i >= 1; --i) {
    float grow = static_cast<float>(i) * 6.f;
    sf::RectangleShape s({r.width + 2.f * grow, r.height + 2.f * grow});
    s.setPosition(snapf(r.left - grow), snapf(r.top - grow));
    sf::Color sc = colShadow;
    sc.a = static_cast<sf::Uint8>(30 * i);
    s.setFillColor(sc);
    t.draw(s);
  }
}

template <typename T>
bool contains(const sf::Rect<T>& r, sf::Vector2f p) {
  return r.contains(p);
}

std::vector<BotType> availableBots() {
  return {BotType::Lilia};
}
std::string botDisplayName(BotType t) {
  return getBotConfig(t).info.name;
}

// tiny FEN validator
bool basicFenCheck(const std::string& fen) {
  std::istringstream ss(fen);
  std::string fields[6];
  for (int i = 0; i < 6; ++i)
    if (!(ss >> fields[i])) return false;
  std::string extra;
  if (ss >> extra) return false;
  {
    int rankCount = 0, i = 0;
    while (i < static_cast<int>(fields[0].size())) {
      int fileSum = 0;
      while (i < static_cast<int>(fields[0].size()) && fields[0][i] != '/') {
        char c = fields[0][i++];
        if (std::isdigit(static_cast<unsigned char>(c))) {
          int n = c - '0';
          if (n <= 0 || n > 8) return false;
          fileSum += n;
        } else {
          switch (c) {
            case 'p':
            case 'r':
            case 'n':
            case 'b':
            case 'q':
            case 'k':
            case 'P':
            case 'R':
            case 'N':
            case 'B':
            case 'Q':
            case 'K':
              fileSum += 1;
              break;
            default:
              return false;
          }
        }
        if (fileSum > 8) return false;
      }
      if (fileSum != 8) return false;
      ++rankCount;
      if (i < static_cast<int>(fields[0].size()) && fields[0][i] == '/') ++i;
    }
    if (rankCount != 8) return false;
  }
  if (!(fields[1] == "w" || fields[1] == "b")) return false;
  if (!(fields[2] == "-" || fields[2].find_first_not_of("KQkq") == std::string::npos)) return false;
  if (!(fields[3] == "-")) {
    if (fields[3].size() != 2) return false;
    if (fields[3][0] < 'a' || fields[3][0] > 'h') return false;
    if (!(fields[3][1] == '3' || fields[3][1] == '6')) return false;
  }
  auto isNonNegInt = [](const std::string& s) {
    if (s.empty()) return false;
    for (char c : s)
      if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    return true;
  };
  if (!isNonNegInt(fields[4])) return false;
  if (!isNonNegInt(fields[5])) return false;
  if (std::stoi(fields[5]) <= 0) return false;
  return true;
}

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

// --- Time helpers ---
inline std::string formatHMS(int totalSeconds) {
  totalSeconds = std::max(0, totalSeconds);
  int h = totalSeconds / 3600;
  int m = (totalSeconds % 3600) / 60;
  int s = totalSeconds % 60;
  std::ostringstream ss;
  ss << (h < 10 ? "0" : "") << h << ":" << (m < 10 ? "0" : "") << m << ":" << (s < 10 ? "0" : "")
     << s;
  return ss.str();
}
inline int clampBaseSeconds(int sec) {
  return std::clamp(sec, 60, 2 * 60 * 60);
}  // 1m..2h
inline int clampIncSeconds(int sec) {
  return std::clamp(sec, 0, 30);
}  // 0..30s

}  // namespace

// ---------------------- class impl ----------------------

StartScreen::StartScreen(sf::RenderWindow& window) : m_window(window) {
  // Fonts / logo
  m_font.loadFromFile(constant::STR_FILE_PATH_FONT);
  m_logoTex.loadFromFile(constant::STR_FILE_PATH_ICON_LILIA_START_SCREEN);  // safe if missing
  m_logo.setTexture(m_logoTex);

  // Default FEN
  m_fenString = core::START_FEN;

  // Time defaults (5|0)
  m_baseSeconds = 300;
  m_incrementSeconds = 0;

  setupUI();
}

void StartScreen::setupUI() {
  const sf::Vector2u ws = m_window.getSize();

  // Panel anchor for initial layout
  sf::Vector2f panelPos((ws.x - PANEL_W) * 0.5f, (ws.y - PANEL_H) * 0.5f);
  m_whitePlayerBtn.setPosition(panelPos.x + 40.f, panelPos.y + 120.f);  // layout anchor

  // Headings
  m_whiteLabel.setFont(m_font);
  m_whiteLabel.setString("White");
  m_whiteLabel.setCharacterSize(22);
  m_whiteLabel.setFillColor(colText);

  m_blackLabel.setFont(m_font);
  m_blackLabel.setString("Black");
  m_blackLabel.setCharacterSize(22);
  m_blackLabel.setFillColor(colText);

  // Buttons - White
  m_whitePlayerBtn.setSize({BTN_W, BTN_H});
  m_whiteBotBtn.setSize({BTN_W, BTN_H});
  m_whitePlayerBtn.setFillColor(colButton);
  m_whiteBotBtn.setFillColor(colButton);
  m_whitePlayerBtn.setOutlineThickness(1.0f);
  m_whiteBotBtn.setOutlineThickness(1.0f);
  m_whitePlayerBtn.setOutlineColor(sf::Color(0, 0, 0, 0));
  m_whiteBotBtn.setOutlineColor(sf::Color(0, 0, 0, 0));

  m_whitePlayerText.setFont(m_font);
  m_whiteBotText.setFont(m_font);
  m_whitePlayerText.setCharacterSize(18);
  m_whiteBotText.setCharacterSize(18);
  m_whitePlayerText.setFillColor(colText);
  m_whiteBotText.setFillColor(colText);
  m_whitePlayerText.setString("Human");
  m_whiteBotText.setString(botDisplayName(BotType::Lilia));

  // Buttons - Black
  m_blackPlayerBtn.setSize({BTN_W, BTN_H});
  m_blackBotBtn.setSize({BTN_W, BTN_H});
  m_blackPlayerBtn.setFillColor(colButton);
  m_blackBotBtn.setFillColor(colButton);
  m_blackPlayerBtn.setOutlineThickness(1.0f);
  m_blackBotBtn.setOutlineThickness(1.0f);
  m_blackPlayerBtn.setOutlineColor(sf::Color(0, 0, 0, 0));
  m_blackBotBtn.setOutlineColor(sf::Color(0, 0, 0, 0));

  m_blackPlayerText.setFont(m_font);
  m_blackBotText.setFont(m_font);
  m_blackPlayerText.setCharacterSize(18);
  m_blackBotText.setCharacterSize(18);
  m_blackPlayerText.setFillColor(colText);
  m_blackBotText.setFillColor(colText);
  m_blackPlayerText.setString("Human");
  m_blackBotText.setString(botDisplayName(BotType::Lilia));

  // Start button (rectangular)
  m_startBtn.setSize({260.f, 54.f});
  m_startBtn.setFillColor(colAccent);
  m_startBtn.setOutlineThickness(0);
  m_startText.setFont(m_font);
  m_startText.setString("Start Game");
  m_startText.setCharacterSize(22);
  m_startText.setFillColor(sf::Color::Black);

  // Tip
  m_creditText.setFont(m_font);
  m_creditText.setCharacterSize(14);
  m_creditText.setFillColor(colSubtle);
  m_creditText.setString("Press F to edit FEN");

  // Arrange positions (snap them)
  float x0 = (ws.x - PANEL_W) * 0.5f;
  float y0 = (ws.y - PANEL_H) * 0.5f;

  m_whiteLabel.setPosition(snapf(x0 + 80.f), snapf(y0 + 100.f));
  m_blackLabel.setPosition(snapf(x0 + PANEL_W - 80.f - m_blackLabel.getLocalBounds().width),
                           snapf(y0 + 100.f));

  // White buttons (left column)
  m_whitePlayerBtn.setPosition(snapf(x0 + 60.f), snapf(y0 + 150.f));
  m_whiteBotBtn.setPosition(snapf(x0 + 60.f), snapf(y0 + 208.f));

  // Black buttons (right column)
  m_blackPlayerBtn.setPosition(snapf(x0 + PANEL_W - 60.f - BTN_W), snapf(y0 + 150.f));
  m_blackBotBtn.setPosition(snapf(x0 + PANEL_W - 60.f - BTN_W), snapf(y0 + 208.f));

  // Start button (center-bottom)
  m_startBtn.setPosition(snapf(x0 + (PANEL_W - m_startBtn.getSize().x) / 2.f),
                         snapf(y0 + PANEL_H - 120.f));
  centerText(m_startText, m_startBtn.getGlobalBounds(), 0.f);

  // Tip (panel bottom-left)
  m_creditText.setPosition(snapf(x0 + 24.f), snapf(y0 + PANEL_H - 40.f));

  // FEN popup (rectangular glass)
  m_fenPopup.setSize({POPUP_W, POPUP_H});
  m_fenPopup.setFillColor(colPanel);
  m_fenPopup.setOutlineThickness(0.f);
  m_fenPopup.setPosition(snapf((ws.x - POPUP_W) / 2.f), snapf((ws.y - POPUP_H) / 2.f));

  m_fenInputBox.setSize({POPUP_W - 60.f - 90.f, 44.f});  // leave room for label
  m_fenInputBox.setFillColor(colInput);
  m_fenInputBox.setOutlineThickness(2.f);
  m_fenInputBox.setOutlineColor(colInputBorder);
  m_fenInputBox.setPosition(snapf(m_fenPopup.getPosition().x + 90.f),
                            snapf(m_fenPopup.getPosition().y + 90.f));

  m_fenInputText.setFont(m_font);
  m_fenInputText.setCharacterSize(18);
  m_fenInputText.setFillColor(colText);
  m_fenInputText.setString(m_fenString);
  leftCenterText(m_fenInputText, m_fenInputBox.getGlobalBounds(), 10.f);

  m_fenBackBtn.setSize({120.f, 40.f});
  m_fenContinueBtn.setSize({140.f, 40.f});
  m_fenBackBtn.setFillColor(colButton);
  m_fenContinueBtn.setFillColor(colAccent);
  float by = m_fenPopup.getPosition().y + POPUP_H - 60.f;
  m_fenBackBtn.setPosition(snapf(m_fenPopup.getPosition().x + 20.f), snapf(by));
  m_fenContinueBtn.setPosition(
      snapf(m_fenPopup.getPosition().x + POPUP_W - 20.f - m_fenContinueBtn.getSize().x), snapf(by));

  m_fenBackText.setFont(m_font);
  m_fenBackText.setCharacterSize(18);
  m_fenBackText.setString("Back");
  m_fenBackText.setFillColor(colText);
  centerText(m_fenBackText, m_fenBackBtn.getGlobalBounds());

  m_fenContinueText.setFont(m_font);
  m_fenContinueText.setCharacterSize(18);
  m_fenContinueText.setString("Continue");
  m_fenContinueText.setFillColor(sf::Color::Black);
  centerText(m_fenContinueText, m_fenContinueBtn.getGlobalBounds());

  m_fenErrorText.setFont(m_font);
  m_fenErrorText.setCharacterSize(16);
  m_fenErrorText.setFillColor(sf::Color(255, 90, 90, 0));
  m_fenErrorText.setString("Incorrect FEN");
  m_fenErrorText.setPosition(snapf(m_fenInputBox.getPosition().x),
                             snapf(m_fenInputBox.getPosition().y + 52.f));

  // Build bot option lists (simple vertical lists)
  auto bots = availableBots();
  auto buildList = [&](std::vector<BotOption>& out, float left, float top) {
    out.clear();
    for (std::size_t i = 0; i < bots.size(); ++i) {
      BotOption opt;
      opt.type = bots[i];
      opt.box.setSize({BTN_W, LIST_ITEM_H});
      opt.box.setPosition(snapf(left), snapf(top + static_cast<float>(i) * (LIST_ITEM_H + 6.f)));
      opt.box.setFillColor(colButton);
      opt.label.setFont(m_font);
      opt.label.setCharacterSize(16);
      opt.label.setString(botDisplayName(bots[i]));
      opt.label.setFillColor(colText);
      leftCenterText(opt.label, opt.box.getGlobalBounds(), 10.f);
      out.push_back(opt);
    }
  };
  buildList(m_whiteBotOptions, m_whiteBotBtn.getPosition().x,
            m_whiteBotBtn.getPosition().y + BTN_H + 8.f);
  buildList(m_blackBotOptions, m_blackBotBtn.getPosition().x,
            m_blackBotBtn.getPosition().y + BTN_H + 8.f);

  // --- Time Control UI (center of main panel) ---
  const float timeX = x0 + (PANEL_W - TIME_W) * 0.5f;
  const float timeY = y0 + (PANEL_H - TIME_H) * 0.5f;

  m_timePanel.setSize({TIME_W, TIME_H});
  m_timePanel.setPosition(snap({timeX, timeY}));
  m_timePanel.setFillColor(sf::Color(42, 48, 63));
  m_timePanel.setOutlineThickness(1.f);
  m_timePanel.setOutlineColor(colPanelBorder);

  m_timeTitle.setFont(m_font);
  m_timeTitle.setCharacterSize(14);
  m_timeTitle.setFillColor(colSubtle);
  m_timeTitle.setString("Time Control");
  m_timeTitle.setPosition(snap({timeX + 10.f, timeY + 8.f}));

  // main time display
  m_timeMain.setFont(m_font);
  m_timeMain.setCharacterSize(22);
  m_timeMain.setFillColor(colText);
  m_timeMain.setString(formatHMS(m_baseSeconds));

  // base steppers
  m_timeMinusBtn.setSize({28.f, 26.f});
  m_timePlusBtn.setSize({28.f, 26.f});
  m_timeMinusBtn.setFillColor(colButton);
  m_timePlusBtn.setFillColor(colButton);

  m_minusTxt.setFont(m_font);
  m_minusTxt.setCharacterSize(18);
  m_minusTxt.setFillColor(colText);
  m_minusTxt.setString("-");
  m_plusTxt = m_minusTxt;
  m_plusTxt.setString("+");

  // increment row
  m_incLabel.setFont(m_font);
  m_incLabel.setCharacterSize(12);
  m_incLabel.setFillColor(colSubtle);
  m_incLabel.setString("Increment");

  m_incValue.setFont(m_font);
  m_incValue.setCharacterSize(16);
  m_incValue.setFillColor(colText);
  m_incValue.setString("+" + std::to_string(m_incrementSeconds) + "s");

  m_incMinusBtn.setSize({24.f, 22.f});
  m_incPlusBtn.setSize({24.f, 22.f});
  m_incMinusBtn.setFillColor(colButton);
  m_incPlusBtn.setFillColor(colButton);

  m_incMinusTxt.setFont(m_font);
  m_incMinusTxt.setCharacterSize(16);
  m_incMinusTxt.setFillColor(colText);
  m_incMinusTxt.setString("-");
  m_incPlusTxt = m_incMinusTxt;
  m_incPlusTxt.setString("+");

  // Preset chips (below the compact box)
  m_presets.clear();
  auto makeChip = [&](const char* label, int base, int inc) {
    PresetChip c;
    float chipW = 74.f;  // compact width to fit three nicely
    c.box.setSize({chipW, CHIP_H});
    c.box.setFillColor(colButton);
    c.box.setOutlineThickness(1.f);
    c.box.setOutlineColor(colPanelBorder);
    c.label.setFont(m_font);
    c.label.setCharacterSize(13);
    c.label.setFillColor(colText);
    c.label.setString(label);
    c.base = base;
    c.inc = inc;
    m_presets.push_back(std::move(c));
  };
  makeChip("Bullet", 60, 0);
  makeChip("Blitz", 180, 2);
  makeChip("Rapid", 600, 0);

  // layout internals
  auto layoutTimeControls = [&]() {
    const sf::Vector2f p = m_timePanel.getPosition();

    // Row 1: [-]  HH:MM:SS  [+]
    float row1Y = p.y + 42.f;
    const float gap = 10.f;
    const float mw = m_timeMinusBtn.getSize().x;
    const float pw = m_timePlusBtn.getSize().x;
    auto mb = m_timeMain.getLocalBounds();
    float totalW = mw + gap + mb.width + gap + pw;
    float left = p.x + (TIME_W - totalW) * 0.5f;

    m_timeMinusBtn.setPosition(snap({left, row1Y - m_timeMinusBtn.getSize().y * 0.5f}));
    m_timePlusBtn.setPosition(
        snap({left + mw + gap + mb.width + gap, row1Y - m_timePlusBtn.getSize().y * 0.5f}));

    sf::FloatRect minusGB = m_timeMinusBtn.getGlobalBounds();
    sf::FloatRect plusGB = m_timePlusBtn.getGlobalBounds();
    sf::FloatRect midBox(minusGB.left + minusGB.width + gap, row1Y - 14.f, mb.width, 28.f);
    centerText(m_timeMain, midBox);
    centerText(m_minusTxt, m_timeMinusBtn.getGlobalBounds());
    centerText(m_plusTxt, m_timePlusBtn.getGlobalBounds());

    // Row 2: Increment label + value [+/-] on the right
    float row2Y = p.y + 80.f;
    m_incLabel.setPosition(snap({p.x + 10.f, row2Y - 9.f}));

    const float incRight = p.x + TIME_W - 10.f;
    m_incPlusBtn.setPosition(
        snap({incRight - m_incPlusBtn.getSize().x, row2Y - m_incPlusBtn.getSize().y * 0.5f}));
    m_incMinusBtn.setPosition(snap({m_incPlusBtn.getPosition().x - 6.f - m_incMinusBtn.getSize().x,
                                    row2Y - m_incMinusBtn.getSize().y * 0.5f}));

    sf::FloatRect incValBox(m_incMinusBtn.getPosition().x - 6.f - 58.f, row2Y - 12.f, 58.f, 24.f);
    centerText(m_incValue, incValBox);
    centerText(m_incMinusTxt, m_incMinusBtn.getGlobalBounds());
    centerText(m_incPlusTxt, m_incPlusBtn.getGlobalBounds());

    // Presets row under the box, centered
    float yChips = p.y + TIME_H + 12.f;
    float chipsTotalW = 3.f * m_presets[0].box.getSize().x + 2.f * CHIP_GAP;
    float chipsLeft = p.x + (TIME_W - chipsTotalW) * 0.5f;
    for (std::size_t i = 0; i < m_presets.size(); ++i) {
      float x = chipsLeft + i * (m_presets[i].box.getSize().x + CHIP_GAP);
      m_presets[i].box.setPosition(snap({x, yChips}));
      centerText(m_presets[i].label, m_presets[i].box.getGlobalBounds(), -1.f);
    }
  };

  // initialize text strings and layout
  m_timeMain.setString(formatHMS(m_baseSeconds));
  m_incValue.setString("+" + std::to_string(m_incrementSeconds) + "s");
  layoutTimeControls();
}

// rectangular panel with soft shadow and border
static void drawPanelWithShadow(sf::RenderWindow& win, const sf::Vector2f& topLeft) {
  sf::FloatRect rect(topLeft.x, topLeft.y, PANEL_W, PANEL_H);
  drawSoftShadowRect(win, rect);
  sf::RectangleShape border({rect.width + 2.f, rect.height + 2.f});
  border.setPosition(snapf(rect.left - 1.f), snapf(rect.top - 1.f));
  border.setFillColor(colPanelBorder);
  win.draw(border);
  sf::RectangleShape panel({rect.width, rect.height});
  panel.setPosition(snapf(rect.left), snapf(rect.top));
  panel.setFillColor(colPanel);
  win.draw(panel);
}

bool StartScreen::handleMouse(sf::Vector2f pos, StartConfig& cfg) {
  // toggle white side
  if (contains(m_whitePlayerBtn.getGlobalBounds(), pos)) {
    cfg.whiteIsBot = false;
    m_showWhiteBotList = false;
    return false;
  }
  if (contains(m_whiteBotBtn.getGlobalBounds(), pos)) {
    cfg.whiteIsBot = true;
    m_showWhiteBotList = !m_showWhiteBotList;
    m_whiteListForceHide = false;
    return false;
  }
  if (m_showWhiteBotList) {
    for (std::size_t i = 0; i < m_whiteBotOptions.size(); ++i) {
      if (contains(m_whiteBotOptions[i].box.getGlobalBounds(), pos)) {
        m_whiteBotSelection = i;
        cfg.whiteBot = m_whiteBotOptions[i].type;
        m_whiteBotText.setString(botDisplayName(cfg.whiteBot));
        m_showWhiteBotList = false;
        break;
      }
    }
  }

  // toggle black side
  if (contains(m_blackPlayerBtn.getGlobalBounds(), pos)) {
    cfg.blackIsBot = false;
    m_showBlackBotList = false;
    return false;
  }
  if (contains(m_blackBotBtn.getGlobalBounds(), pos)) {
    cfg.blackIsBot = true;
    m_showBlackBotList = !m_showBlackBotList;
    m_blackListForceHide = false;
    return false;
  }
  if (m_showBlackBotList) {
    for (std::size_t i = 0; i < m_blackBotOptions.size(); ++i) {
      if (contains(m_blackBotOptions[i].box.getGlobalBounds(), pos)) {
        m_blackBotSelection = i;
        cfg.blackBot = m_blackBotOptions[i].type;
        m_blackBotText.setString(botDisplayName(cfg.blackBot));
        m_showBlackBotList = false;
        break;
      }
    }
  }

  // presets (time) — handled here for click; hold steppers handled in run()
  for (auto& chip : m_presets) {
    const bool hit = contains(chip.box.getGlobalBounds(), pos);
    if (hit) {
      chip.box.setFillColor(colButtonActive);
      chip.box.setOutlineColor(colAccent);
      m_baseSeconds = clampBaseSeconds(chip.base);
      m_incrementSeconds = clampIncSeconds(chip.inc);
      m_timeMain.setString(formatHMS(m_baseSeconds));
      m_incValue.setString("+" + std::to_string(m_incrementSeconds) + "s");
      return false;
    } else {
      chip.box.setFillColor(colButton);
      chip.box.setOutlineColor(colPanelBorder);
    }
  }

  // start
  if (contains(m_startBtn.getGlobalBounds(), pos)) {
    return true;
  }

  // clicking elsewhere hides the lists
  if (!contains(m_whiteBotBtn.getGlobalBounds(), pos)) m_showWhiteBotList = false;
  if (!contains(m_blackBotBtn.getGlobalBounds(), pos)) m_showBlackBotList = false;

  return false;
}

bool StartScreen::handleFenMouse(sf::Vector2f pos, StartConfig& cfg) {
  (void)pos;
  (void)cfg;
  return false;
}

bool StartScreen::isValidFen(const std::string& fen) {
  return basicFenCheck(fen);
}

// --- helper: process press&hold repeating ---
void StartScreen::processHoldRepeater(HoldRepeater& r, const sf::FloatRect& bounds,
                                      sf::Vector2f mouse, std::function<void()> stepFn,
                                      float initialDelay, float repeatRate) {
  if (!r.active) return;
  if (!bounds.contains(mouse)) return;  // only repeat while cursor stays over the control
  float t = r.clock.getElapsedTime().asSeconds();
  if (t < initialDelay) return;
  int ticks = static_cast<int>((t - initialDelay) / repeatRate);
  while (r.fired < ticks) {
    stepFn();
    r.fired++;
  }
}

StartConfig StartScreen::run() {
  StartConfig cfg;
  cfg.whiteIsBot = false;
  cfg.blackIsBot = true;
  cfg.whiteBot = BotType::Lilia;
  cfg.blackBot = BotType::Lilia;
  cfg.fen = m_fenString;  // default prefill
  cfg.timeBaseSeconds = m_baseSeconds;
  cfg.timeIncrementSeconds = m_incrementSeconds;

  // hover visuals
  auto hoverButton = [&](sf::RectangleShape& btn, sf::Vector2f mouse) {
    btn.setFillColor(contains(btn.getGlobalBounds(), mouse) ? colButtonHover : colButton);
  };

  // Toast (unused now but kept if you want it later)
  bool toastVisible = false;
  sf::Clock toastClock;
  std::string toastMsg = "Using standard start position";
  auto showToast = [&]() {
    toastVisible = true;
    toastClock.restart();
  };

  auto drawUI = [&]() {
    drawVerticalGradient(m_window, colBGTop, colBGBottom);

    // faint logo top-right, no rotation
    if (m_logoTex.getSize().x > 0 && m_logoTex.getSize().y > 0) {
      sf::Sprite logoBG(m_logoTex);
      const auto ws = m_window.getSize();
      const float desiredH = ws.y * 0.90f;
      const float s = desiredH / static_cast<float>(m_logoTex.getSize().y);
      logoBG.setScale(s, s);
      const sf::FloatRect lb = logoBG.getLocalBounds();
      logoBG.setOrigin(lb.width, 0.f);
      const float pad = 24.f;
      logoBG.setPosition(snapf(static_cast<float>(ws.x) - pad), snapf(pad));
      logoBG.setRotation(0.f);
      logoBG.setColor(sf::Color(150, 120, 255, 70));
      m_window.draw(logoBG, sf::RenderStates(sf::BlendAlpha));
    }

    // main panel
    sf::Vector2f panelPos((m_window.getSize().x - PANEL_W) * 0.5f,
                          (m_window.getSize().y - PANEL_H) * 0.5f);
    drawPanelWithShadow(m_window, panelPos);

    // header
    sf::Text title("Lilia Engine - Bot Sandbox", m_font, 28);
    title.setFillColor(colText);
    title.setPosition(snapf(panelPos.x + 24.f), snapf(panelPos.y + 18.f));
    m_window.draw(title);

    sf::Text subtitle("Try different chess bots. Choose sides & engine.", m_font, 18);
    subtitle.setFillColor(colSubtle);
    subtitle.setPosition(snapf(panelPos.x + 24.f), snapf(panelPos.y + 52.f));
    m_window.draw(subtitle);

    // Section labels
    m_window.draw(m_whiteLabel);
    m_window.draw(m_blackLabel);

    // Selected state styling
    auto applyToggleStyle = [&](sf::RectangleShape& humanBtn, sf::RectangleShape& botBtn,
                                bool isBot) {
      humanBtn.setOutlineThickness(1.f);
      botBtn.setOutlineThickness(1.f);
      humanBtn.setOutlineColor(sf::Color(0, 0, 0, 0));
      botBtn.setOutlineColor(sf::Color(0, 0, 0, 0));
      humanBtn.setFillColor(colButton);
      botBtn.setFillColor(colButton);
      if (isBot) {
        botBtn.setFillColor(colButtonActive);
        botBtn.setOutlineThickness(2.f);
        botBtn.setOutlineColor(colAccent);
      } else {
        humanBtn.setFillColor(colButtonActive);
        humanBtn.setOutlineThickness(2.f);
        humanBtn.setOutlineColor(colAccent);
      }
    };
    applyToggleStyle(m_whitePlayerBtn, m_whiteBotBtn, cfg.whiteIsBot);
    applyToggleStyle(m_blackPlayerBtn, m_blackBotBtn, cfg.blackIsBot);

    // White column
    m_window.draw(m_whitePlayerBtn);
    m_window.draw(m_whiteBotBtn);
    centerText(m_whitePlayerText, m_whitePlayerBtn.getGlobalBounds());
    centerText(m_whiteBotText, m_whiteBotBtn.getGlobalBounds());
    m_window.draw(m_whitePlayerText);
    m_window.draw(m_whiteBotText);

    // Black column
    m_window.draw(m_blackPlayerBtn);
    m_window.draw(m_blackBotBtn);
    centerText(m_blackPlayerText, m_blackPlayerBtn.getGlobalBounds());
    centerText(m_blackBotText, m_blackBotBtn.getGlobalBounds());
    m_window.draw(m_blackPlayerText);
    m_window.draw(m_blackBotText);

    // Bot dropdown lists
    auto drawBotList = [&](const std::vector<BotOption>& list, std::size_t selIdx) {
      for (std::size_t i = 0; i < list.size(); ++i) {
        const auto& opt = list[i];
        sf::RectangleShape box = opt.box;
        if (i == selIdx) {
          box.setFillColor(colButtonActive);
          box.setOutlineThickness(2.f);
          box.setOutlineColor(colAccent);
        } else {
          box.setFillColor(colButton);
          box.setOutlineThickness(0.f);
        }
        m_window.draw(box);
        sf::Text label = opt.label;
        leftCenterText(label, box.getGlobalBounds(), 10.f);
        m_window.draw(label);
      }
    };
    if (m_showWhiteBotList) drawBotList(m_whiteBotOptions, m_whiteBotSelection);
    if (m_showBlackBotList) drawBotList(m_blackBotOptions, m_blackBotSelection);

    // --- Time Panel (compact, centered) ---
    {
      auto gb = m_timePanel.getGlobalBounds();
      drawSoftShadowRect(m_window, gb);
      m_window.draw(m_timePanel);
      m_window.draw(m_timeTitle);

      // hover fill for steppers
      auto hoverColorize = [&](sf::RectangleShape& r) {
        bool hov = contains(r.getGlobalBounds(), m_mousePos);
        r.setFillColor(hov ? colButtonHover : colButton);
      };
      hoverColorize(m_timeMinusBtn);
      hoverColorize(m_timePlusBtn);
      hoverColorize(m_incMinusBtn);
      hoverColorize(m_incPlusBtn);

      // draw time controls
      m_window.draw(m_timeMinusBtn);
      m_window.draw(m_timePlusBtn);
      m_window.draw(m_minusTxt);
      m_window.draw(m_plusTxt);
      m_window.draw(m_timeMain);

      // increment row
      m_window.draw(m_incLabel);
      m_window.draw(m_incMinusBtn);
      m_window.draw(m_incPlusBtn);
      m_window.draw(m_incMinusTxt);
      m_window.draw(m_incPlusTxt);
      m_window.draw(m_incValue);

      // presets
      for (auto& c : m_presets) {
        // subtle hover raise
        bool hov = contains(c.box.getGlobalBounds(), m_mousePos);
        if (!c.box.getGlobalBounds().contains(m_mousePos)) c.box.setFillColor(colButton);
        if (hov) c.box.setFillColor(colButtonHover);
        m_window.draw(c.box);
        m_window.draw(c.label);
      }
    }

    // Start button (shadow + body)
    {
      const sf::FloatRect gb = m_startBtn.getGlobalBounds();
      drawSoftShadowRect(m_window, {gb.left, gb.top, gb.width, gb.height});
      sf::RectangleShape body({gb.width, gb.height});
      body.setPosition(snapf(gb.left), snapf(gb.top));
      body.setFillColor(m_startBtn.getFillColor());
      m_window.draw(body);
      centerText(m_startText, gb);
      m_window.draw(m_startText);
    }

    // Tip
    m_window.draw(m_creditText);
  };

  // Popup drawing (unchanged except placeholder text fix kept earlier)
  auto drawFenPopup = [&]() {
    sf::RectangleShape overlay(
        {static_cast<float>(m_window.getSize().x), static_cast<float>(m_window.getSize().y)});
    overlay.setPosition(0.f, 0.f);
    overlay.setFillColor(sf::Color(0, 0, 0, 180));
    m_window.draw(overlay);

    sf::Vector2f p = m_fenPopup.getPosition();
    sf::FloatRect rect(p.x, p.y, POPUP_W, POPUP_H);
    drawSoftShadowRect(m_window, rect);

    sf::RectangleShape border({rect.width + 2.f, rect.height + 2.f});
    border.setPosition(snapf(rect.left - 1.f), snapf(rect.top - 1.f));
    border.setFillColor(colPanelBorder);
    m_window.draw(border);

    sf::RectangleShape body({rect.width, rect.height});
    body.setPosition(snapf(rect.left), snapf(rect.top));
    body.setFillColor(colTextPanel);
    m_window.draw(body);

    sf::Text title("Custom Position", m_font, 22);
    title.setFillColor(colText);
    title.setPosition(snapf(p.x + 20.f), snapf(p.y + 18.f));
    m_window.draw(title);

    sf::Text label("FEN:", m_font, 18);
    label.setFillColor(colSubtle);
    label.setPosition(snapf(p.x + 20.f), snapf(m_fenInputBox.getPosition().y + 10.f));
    m_window.draw(label);

    m_window.draw(m_fenInputBox);

    const float padX = 10.f;
    const float usableW = m_fenInputBox.getSize().x - (padX * 2.f);

    std::string toShow;
    bool isEmpty = m_fenString.empty();
    if (!isEmpty) toShow = ellipsizeRightKeepTail(m_fenString, m_fenInputText, usableW);

    m_fenInputText.setFillColor(isEmpty ? colSubtle : colText);
    m_fenInputText.setString(isEmpty ? "Paste or type a FEN..." : toShow);
    leftCenterText(m_fenInputText, m_fenInputBox.getGlobalBounds(), padX);
    m_window.draw(m_fenInputText);

    centerText(m_fenBackText, m_fenBackBtn.getGlobalBounds());
    centerText(m_fenContinueText, m_fenContinueBtn.getGlobalBounds());
    m_window.draw(m_fenBackBtn);
    m_window.draw(m_fenBackText);
    m_window.draw(m_fenContinueBtn);
    m_window.draw(m_fenContinueText);
  };

  // Input focus for popup
  static bool fenInputActive = false;
  (void)fenInputActive;

  // loop
  while (m_window.isOpen()) {
    sf::Event e{};
    while (m_window.pollEvent(e)) {
      if (e.type == sf::Event::Closed) {
        m_window.close();
        break;
      }
      if (e.type == sf::Event::Resized) {
        setupUI();  // re-layout
      }

      if (e.type == sf::Event::MouseMoved) {
        m_mousePos = {static_cast<float>(e.mouseMove.x), static_cast<float>(e.mouseMove.y)};
        // basic button hovers
        hoverButton(m_whitePlayerBtn, m_mousePos);
        hoverButton(m_whiteBotBtn, m_mousePos);
        hoverButton(m_blackPlayerBtn, m_mousePos);
        hoverButton(m_blackBotBtn, m_mousePos);
        if (contains(m_startBtn.getGlobalBounds(), m_mousePos)) {
          m_startBtn.setFillColor(sf::Color(80, 205, 255));
        } else {
          m_startBtn.setFillColor(colAccent);
        }
      }

      if (!m_showFenPopup) {
        // normal screen input
        if (e.type == sf::Event::KeyPressed) {
          if (e.key.code == sf::Keyboard::F) {
            m_showFenPopup = true;
            fenInputActive = true;
            m_fenInputText.setString(m_fenString);
          } else if (e.key.code == sf::Keyboard::Left) {
            m_baseSeconds = clampBaseSeconds(m_baseSeconds - (e.key.shift ? 300 : 60));
            m_timeMain.setString(formatHMS(m_baseSeconds));
          } else if (e.key.code == sf::Keyboard::Right) {
            m_baseSeconds = clampBaseSeconds(m_baseSeconds + (e.key.shift ? 300 : 60));
            m_timeMain.setString(formatHMS(m_baseSeconds));
          } else if (e.key.code == sf::Keyboard::Down) {
            m_incrementSeconds = clampIncSeconds(m_incrementSeconds - 1);
            m_incValue.setString("+" + std::to_string(m_incrementSeconds) + "s");
          } else if (e.key.code == sf::Keyboard::Up) {
            m_incrementSeconds = clampIncSeconds(m_incrementSeconds + 1);
            m_incValue.setString("+" + std::to_string(m_incrementSeconds) + "s");
          } else if (e.key.code == sf::Keyboard::Enter) {
            cfg.timeBaseSeconds = m_baseSeconds;
            cfg.timeIncrementSeconds = m_incrementSeconds;
            return cfg;
          }
        }

        if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
          sf::Vector2f mp(static_cast<float>(e.mouseButton.x), static_cast<float>(e.mouseButton.y));

          // steppers: immediate step + arm hold repeater
          if (contains(m_timeMinusBtn.getGlobalBounds(), mp)) {
            m_baseSeconds = clampBaseSeconds(m_baseSeconds - 60);
            m_timeMain.setString(formatHMS(m_baseSeconds));
            m_holdBaseMinus.active = true;
            m_holdBaseMinus.clock.restart();
            m_holdBaseMinus.fired = 0;
          } else if (contains(m_timePlusBtn.getGlobalBounds(), mp)) {
            m_baseSeconds = clampBaseSeconds(m_baseSeconds + 60);
            m_timeMain.setString(formatHMS(m_baseSeconds));
            m_holdBasePlus.active = true;
            m_holdBasePlus.clock.restart();
            m_holdBasePlus.fired = 0;
          } else if (contains(m_incMinusBtn.getGlobalBounds(), mp)) {
            m_incrementSeconds = clampIncSeconds(m_incrementSeconds - 1);
            m_incValue.setString("+" + std::to_string(m_incrementSeconds) + "s");
            m_holdIncMinus.active = true;
            m_holdIncMinus.clock.restart();
            m_holdIncMinus.fired = 0;
          } else if (contains(m_incPlusBtn.getGlobalBounds(), mp)) {
            m_incrementSeconds = clampIncSeconds(m_incrementSeconds + 1);
            m_incValue.setString("+" + std::to_string(m_incrementSeconds) + "s");
            m_holdIncPlus.active = true;
            m_holdIncPlus.clock.restart();
            m_holdIncPlus.fired = 0;
          } else {
            // delegate to general handler (sides, presets, start)
            if (handleMouse(mp, cfg)) {
              cfg.timeBaseSeconds = m_baseSeconds;
              cfg.timeIncrementSeconds = m_incrementSeconds;
              return cfg;
            }
          }
        }

        if (e.type == sf::Event::MouseButtonReleased && e.mouseButton.button == sf::Mouse::Left) {
          // stop all repeaters
          m_holdBaseMinus.active = m_holdBasePlus.active = m_holdIncMinus.active =
              m_holdIncPlus.active = false;
        }

      } else {
        // popup input
        if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
          sf::Vector2f mp(static_cast<float>(e.mouseButton.x), static_cast<float>(e.mouseButton.y));
          bool clickedBack = contains(m_fenBackBtn.getGlobalBounds(), mp);
          bool clickedContinue = contains(m_fenContinueBtn.getGlobalBounds(), mp);
          bool clickedInput = contains(m_fenInputBox.getGlobalBounds(), mp);
          bool clickedInsidePopup = contains(m_fenPopup.getGlobalBounds(), mp);

          if (clickedBack) {
            m_showFenPopup = false;
            fenInputActive = false;
          } else if (clickedContinue) {
            if (m_fenString.empty() || !isValidFen(m_fenString)) {
              cfg.fen = core::START_FEN;
              m_showFenPopup = false;
              fenInputActive = false;
            } else {
              cfg.fen = m_fenString;
              m_showFenPopup = false;
              fenInputActive = false;
            }
          } else if (clickedInput) {
            fenInputActive = true;
          } else if (!clickedInsidePopup) {
            fenInputActive = false;
          }
        }
        if (e.type == sf::Event::KeyPressed) {
          if (e.key.code == sf::Keyboard::Escape) {
            m_showFenPopup = false;
            fenInputActive = false;
          } else if (e.key.code == sf::Keyboard::Enter) {
            if (m_fenString.empty() || !isValidFen(m_fenString)) {
              cfg.fen = core::START_FEN;
              m_showFenPopup = false;
              fenInputActive = false;
            } else {
              cfg.fen = m_fenString;
              m_showFenPopup = false;
              fenInputActive = false;
            }
          }
        }
        if (fenInputActive && e.type == sf::Event::TextEntered) {
          if (e.text.unicode == 8) {  // backspace
            if (!m_fenString.empty()) m_fenString.pop_back();
          } else if (e.text.unicode >= 32 && e.text.unicode < 127) {
            m_fenString.push_back(static_cast<char>(e.text.unicode));
          }
          m_fenInputText.setString(m_fenString);
        }
      }
    }

    // ---- Press&Hold auto-repeat processing (every frame) ----
    processHoldRepeater(m_holdBaseMinus, m_timeMinusBtn.getGlobalBounds(), m_mousePos, [&] {
      m_baseSeconds = clampBaseSeconds(m_baseSeconds - 60);
      m_timeMain.setString(formatHMS(m_baseSeconds));
    });
    processHoldRepeater(m_holdBasePlus, m_timePlusBtn.getGlobalBounds(), m_mousePos, [&] {
      m_baseSeconds = clampBaseSeconds(m_baseSeconds + 60);
      m_timeMain.setString(formatHMS(m_baseSeconds));
    });
    processHoldRepeater(m_holdIncMinus, m_incMinusBtn.getGlobalBounds(), m_mousePos, [&] {
      m_incrementSeconds = clampIncSeconds(m_incrementSeconds - 1);
      m_incValue.setString("+" + std::to_string(m_incrementSeconds) + "s");
    });
    processHoldRepeater(m_holdIncPlus, m_incPlusBtn.getGlobalBounds(), m_mousePos, [&] {
      m_incrementSeconds = clampIncSeconds(m_incrementSeconds + 1);
      m_incValue.setString("+" + std::to_string(m_incrementSeconds) + "s");
    });

    // draw
    m_window.clear();
    drawUI();
    if (m_showFenPopup) drawFenPopup();
    m_window.display();
  }

  return cfg;  // window closed
}

}  // namespace lilia::view
