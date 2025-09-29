#include "lilia/view/start_screen.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Window/Clipboard.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <unordered_map>

#include "lilia/bot/bot_info.hpp"
#include "lilia/view/color_palette_manager.hpp"
#include "lilia/view/render_constants.hpp"
#include "lilia/view/start_screen_utils.hpp"

namespace lilia::view {

using start_screen::ui::availableBots;
using start_screen::ui::basicFenCheck;
using start_screen::ui::basicPgnCheck;
using start_screen::ui::botDisplayName;
using start_screen::ui::centerText;
using start_screen::ui::clampBaseSeconds;
using start_screen::ui::clampIncSeconds;
using start_screen::ui::contains;
using start_screen::ui::darken;
using start_screen::ui::drawAccentInset;
using start_screen::ui::drawBevelButton3D;
using start_screen::ui::drawVerticalGradient;
using start_screen::ui::formatHMS;
using start_screen::ui::leftCenterText;
using start_screen::ui::lighten;
using start_screen::ui::snap;
using start_screen::ui::snapf;

namespace {
// --------- Layout ---------
constexpr float PANEL_W = 820.f;
constexpr float PANEL_H = 520.f;

constexpr float BTN_H = 44.f;
constexpr float BTN_W = 180.f;

constexpr float LIST_ITEM_H = 36.f;

// Time panel (no shadows now)
constexpr float TIME_W = 200.f;
constexpr float TIME_H = 120.f;
constexpr float CHIP_H = 24.f;
constexpr float CHIP_GAP = 10.f;
constexpr float TOGGLE_W = TIME_W * 0.80f;
constexpr float TOGGLE_H = 30.f;

constexpr float MODAL_W = 560.f;
constexpr float MODAL_H = 320.f;

// Colors sourced from palette manager
#define colBGTop ColorPaletteManager::get().palette().COL_BG_TOP
#define colBGBottom ColorPaletteManager::get().palette().COL_BG_BOTTOM
#define colPanel ColorPaletteManager::get().palette().COL_PANEL_TRANS
#define colTextPanel ColorPaletteManager::get().palette().COL_PANEL
#define colPanelBorder ColorPaletteManager::get().palette().COL_PANEL_BORDER_ALT
#define colButton ColorPaletteManager::get().palette().COL_BUTTON
#define colButtonActive ColorPaletteManager::get().palette().COL_BUTTON_ACTIVE
#define colAccent ColorPaletteManager::get().palette().COL_ACCENT
#define colText ColorPaletteManager::get().palette().COL_TEXT
#define colSubtle ColorPaletteManager::get().palette().COL_MUTED_TEXT
#define colTimeOff ColorPaletteManager::get().palette().COL_TIME_OFF
#define colInput ColorPaletteManager::get().palette().COL_INPUT_BG
#define colInputBorder ColorPaletteManager::get().palette().COL_INPUT_BORDER
#define colValid ColorPaletteManager::get().palette().COL_VALID
#define colInvalid ColorPaletteManager::get().palette().COL_INVALID

}  // namespace

// ---------------------- class impl ----------------------

StartScreen::StartScreen(sf::RenderWindow& window) : m_window(window) {
  m_font.loadFromFile(constant::STR_FILE_PATH_FONT);
  m_logoTex.loadFromFile(constant::STR_FILE_PATH_ICON_LILIA_START_SCREEN);
  m_logo.setTexture(m_logoTex);

  // FEN starts empty => STANDARD unless user provides one
  m_fenString.clear();

  // Time defaults (off)
  m_baseSeconds = 300;
  m_incrementSeconds = 0;
  m_timeEnabled = false;

  setupUI();
  applyTheme();
  m_listener_id = ColorPaletteManager::get().addListener([this]() { applyTheme(); });
}

StartScreen::~StartScreen() {
  ColorPaletteManager::get().removeListener(m_listener_id);
}

void StartScreen::setupUI() {
  const sf::Vector2u ws = m_window.getSize();

  // palette button position and options
  m_paletteText.setFont(m_font);
  m_paletteText.setString("Color Theme");
  m_paletteText.setCharacterSize(16);
  m_paletteText.setFillColor(colText);
  auto tb = m_paletteText.getLocalBounds();
  float pad = 8.f;
  m_paletteButton.setSize({tb.width + pad * 2.f, tb.height + pad * 2.f});
  m_paletteButton.setFillColor(colButton);
  m_paletteButton.setPosition(20.f, ws.y - m_paletteButton.getSize().y - 20.f);
  m_paletteText.setPosition(snapf(m_paletteButton.getPosition().x + pad - tb.left),
                            snapf(m_paletteButton.getPosition().y + pad - tb.top));

  m_paletteOptions.clear();
  float itemH = 24.f;
  float width = 120.f;
  float left = m_paletteButton.getPosition().x - 1.f;
  float bottom = m_paletteButton.getPosition().y;
  const auto& names = ColorPaletteManager::get().paletteNames();
  for (std::size_t i = 0; i < names.size(); ++i) {
    PaletteOption opt;
    opt.name = names[i];
    opt.box.setSize({width, itemH});
    opt.box.setPosition(snap({left, bottom - (i + 1) * itemH}));
    opt.box.setFillColor(colButton);
    opt.label.setFont(m_font);
    opt.label.setCharacterSize(14);
    std::string label = names[i];
    opt.label.setString(label);
    opt.label.setFillColor(colText);
    leftCenterText(opt.label, opt.box.getGlobalBounds(), 8.f);
    m_paletteOptions.push_back(opt);
  }
  const std::string& activeName = ColorPaletteManager::get().activePalette();
  m_paletteSelection = 0;
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (names[i] == activeName) {
      m_paletteSelection = i;
      break;
    }
  }

  // Headings
  m_whiteLabel.setFont(m_font);
  m_whiteLabel.setString("White");
  m_whiteLabel.setCharacterSize(22);
  m_whiteLabel.setFillColor(colText);
  m_blackLabel.setFont(m_font);
  m_blackLabel.setString("Black");
  m_blackLabel.setCharacterSize(22);
  m_blackLabel.setFillColor(colText);

  // Player/Bot buttons
  auto initSideBtns = [&](sf::RectangleShape& humanBtn, sf::RectangleShape& botBtn,
                          sf::Text& humanTxt, sf::Text& botTxt) {
    humanBtn.setSize({BTN_W, BTN_H});
    botBtn.setSize({BTN_W, BTN_H});
    humanBtn.setFillColor(colButton);
    botBtn.setFillColor(colButton);
    humanBtn.setOutlineThickness(0.f);
    botBtn.setOutlineThickness(0.f);
    humanTxt.setFont(m_font);
    botTxt.setFont(m_font);
    humanTxt.setCharacterSize(18);
    botTxt.setCharacterSize(18);
    humanTxt.setFillColor(colText);
    botTxt.setFillColor(colText);
    humanTxt.setString("Human");
    botTxt.setString(botDisplayName(BotType::Lilia));
  };
  initSideBtns(m_whitePlayerBtn, m_whiteBotBtn, m_whitePlayerText, m_whiteBotText);
  initSideBtns(m_blackPlayerBtn, m_blackBotBtn, m_blackPlayerText, m_blackBotText);

  // Start
  m_startBtn.setSize({260.f, 54.f});
  m_startBtn.setFillColor(colAccent);
  m_startBtn.setOutlineThickness(0);
  m_startText.setFont(m_font);
  m_startText.setString("Start Game");
  m_startText.setCharacterSize(22);
  m_startText.setFillColor(constant::COL_DARK_TEXT);

  // Layout anchors
  float x0 = (ws.x - PANEL_W) * 0.5f;
  float y0 = (ws.y - PANEL_H) * 0.5f;

  m_whiteLabel.setPosition(snapf(x0 + 80.f), snapf(y0 + 100.f));
  m_blackLabel.setPosition(snapf(x0 + PANEL_W - 80.f - m_blackLabel.getLocalBounds().width),
                           snapf(y0 + 100.f));

  m_whitePlayerBtn.setPosition(snapf(x0 + 60.f), snapf(y0 + 150.f));
  m_whiteBotBtn.setPosition(snapf(x0 + 60.f), snapf(y0 + 208.f));

  m_blackPlayerBtn.setPosition(snapf(x0 + PANEL_W - 60.f - BTN_W), snapf(y0 + 150.f));
  m_blackBotBtn.setPosition(snapf(x0 + PANEL_W - 60.f - BTN_W), snapf(y0 + 208.f));

  m_startBtn.setPosition(snapf(x0 + (PANEL_W - m_startBtn.getSize().x) / 2.f),
                         snapf(y0 + PANEL_H - 120.f));
  centerText(m_startText, m_startBtn.getGlobalBounds());

  // Load game button
  m_loadGameBtn.setSize({260.f, 44.f});
  m_loadGameBtn.setFillColor(colButton);
  m_loadGameBtn.setOutlineThickness(0.f);
  m_loadGameBtn.setPosition(
      snap({x0 + (PANEL_W - m_loadGameBtn.getSize().x) * 0.5f,
            m_startBtn.getPosition().y + m_startBtn.getSize().y + 18.f}));

  m_loadGameText.setFont(m_font);
  m_loadGameText.setCharacterSize(20);
  m_loadGameText.setFillColor(colText);
  m_loadGameText.setString("Load Game");
  centerText(m_loadGameText, m_loadGameBtn.getGlobalBounds());

  // Modal layout (precomputed for consistency)
  const sf::Vector2f modalPos((ws.x - MODAL_W) * 0.5f, (ws.y - MODAL_H) * 0.5f);
  const float modalPad = 28.f;
  const float fieldGap = 18.f;

  m_modalBackdrop.setSize(sf::Vector2f(ws));
  m_modalBackdrop.setPosition(0.f, 0.f);
  m_modalBackdrop.setFillColor(sf::Color(0, 0, 0, 140));

  m_modalPanel.setSize({MODAL_W, MODAL_H});
  m_modalPanel.setPosition(snap(modalPos));

  m_modalTitle.setFont(m_font);
  m_modalTitle.setCharacterSize(26);
  m_modalTitle.setString("Load Game");
  m_modalTitle.setFillColor(colText);
  m_modalTitle.setPosition(snap({modalPos.x + modalPad, modalPos.y + modalPad}));

  m_modalSubtitle.setFont(m_font);
  m_modalSubtitle.setCharacterSize(16);
  m_modalSubtitle.setFillColor(colSubtle);
  m_modalSubtitle.setString("Paste a FEN or PGN position to start from.");
  m_modalSubtitle.setPosition(
      snap({modalPos.x + modalPad, m_modalTitle.getPosition().y + 32.f}));

  float currentY = m_modalSubtitle.getPosition().y + 40.f;
  const float fieldWidth = MODAL_W - 2.f * modalPad;

  m_modalFenLabel.setFont(m_font);
  m_modalFenLabel.setCharacterSize(14);
  m_modalFenLabel.setFillColor(colSubtle);
  m_modalFenLabel.setString("FEN (optional)");
  m_modalFenLabel.setPosition(snap({modalPos.x + modalPad, currentY}));

  currentY += 20.f;
  m_fenInputBox.setSize({fieldWidth, 42.f});
  m_fenInputBox.setFillColor(colInput);
  m_fenInputBox.setOutlineThickness(2.f);
  m_fenInputBox.setOutlineColor(colInputBorder);
  m_fenInputBox.setPosition(snap({modalPos.x + modalPad, currentY}));

  m_fenInputText.setFont(m_font);
  m_fenInputText.setCharacterSize(16);
  m_fenInputText.setFillColor(colText);
  m_fenInputText.setString(m_fenString);
  m_fenInputText.setPosition(m_fenInputBox.getPosition().x + 10.f,
                             m_fenInputBox.getPosition().y + m_fenInputBox.getSize().y / 2.f);

  currentY += m_fenInputBox.getSize().y + 4.f;
  m_fenErrorText.setFont(m_font);
  m_fenErrorText.setCharacterSize(13);
  m_fenErrorText.setFillColor(colInvalid);
  m_fenErrorText.setString("");
  m_fenErrorText.setPosition(snap({modalPos.x + modalPad, currentY}));

  currentY += fieldGap;
  m_modalPgnLabel.setFont(m_font);
  m_modalPgnLabel.setCharacterSize(14);
  m_modalPgnLabel.setFillColor(colSubtle);
  m_modalPgnLabel.setString("PGN (optional)");
  m_modalPgnLabel.setPosition(snap({modalPos.x + modalPad, currentY}));

  currentY += 20.f;
  m_pgnInputBox.setSize({fieldWidth, 120.f});
  m_pgnInputBox.setFillColor(colInput);
  m_pgnInputBox.setOutlineThickness(2.f);
  m_pgnInputBox.setOutlineColor(colInputBorder);
  m_pgnInputBox.setPosition(snap({modalPos.x + modalPad, currentY}));

  m_pgnInputText.setFont(m_font);
  m_pgnInputText.setCharacterSize(15);
  m_pgnInputText.setFillColor(colText);
  m_pgnInputText.setLineSpacing(1.2f);
  m_pgnInputText.setString(m_pgnString);
  m_pgnInputText.setPosition(m_pgnInputBox.getPosition().x + 10.f,
                             m_pgnInputBox.getPosition().y + 10.f);

  currentY += m_pgnInputBox.getSize().y + 4.f;
  m_pgnErrorText.setFont(m_font);
  m_pgnErrorText.setCharacterSize(13);
  m_pgnErrorText.setFillColor(colInvalid);
  m_pgnErrorText.setString("");
  m_pgnErrorText.setPosition(snap({modalPos.x + modalPad, currentY}));

  const float buttonY = modalPos.y + MODAL_H - modalPad - 44.f;
  const sf::Vector2f buttonSize(132.f, 44.f);
  m_modalApplyBtn.setSize(buttonSize);
  m_modalApplyBtn.setPosition(
      snap({modalPos.x + MODAL_W - modalPad - buttonSize.x, buttonY}));
  m_modalApplyBtn.setFillColor(colAccent);
  m_modalApplyBtn.setOutlineThickness(0.f);

  m_modalApplyText.setFont(m_font);
  m_modalApplyText.setCharacterSize(18);
  m_modalApplyText.setFillColor(constant::COL_DARK_TEXT);
  m_modalApplyText.setString("Apply");
  centerText(m_modalApplyText, m_modalApplyBtn.getGlobalBounds());

  m_modalCancelBtn.setSize(buttonSize);
  m_modalCancelBtn.setPosition(
      snap({m_modalApplyBtn.getPosition().x - 12.f - buttonSize.x, buttonY}));
  m_modalCancelBtn.setFillColor(colButton);
  m_modalCancelBtn.setOutlineThickness(0.f);

  m_modalCancelText.setFont(m_font);
  m_modalCancelText.setCharacterSize(18);
  m_modalCancelText.setFillColor(colText);
  m_modalCancelText.setString("Cancel");
  centerText(m_modalCancelText, m_modalCancelBtn.getGlobalBounds());

  // Build bot option lists
  auto bots = availableBots();
  auto buildList = [&](std::vector<BotOption>& out, float left, float top) {
    out.clear();
    for (std::size_t i = 0; i < bots.size(); ++i) {
      BotOption opt;
      opt.type = bots[i];
      opt.box.setSize({BTN_W, LIST_ITEM_H});
      opt.box.setPosition(snapf(left), snapf(top + (float)i * LIST_ITEM_H));
      opt.box.setFillColor(colButton);
      opt.label.setFont(m_font);
      opt.label.setCharacterSize(16);
      opt.label.setString(botDisplayName(bots[i]));
      opt.label.setFillColor(colText);
      leftCenterText(opt.label, opt.box.getGlobalBounds(), 10.f);
      out.push_back(opt);
    }
  };
  buildList(m_whiteBotOptions, m_whiteBotBtn.getPosition().x - 1.f,
            m_whiteBotBtn.getPosition().y + BTN_H);
  buildList(m_blackBotOptions, m_blackBotBtn.getPosition().x - 1.f,
            m_blackBotBtn.getPosition().y + BTN_H);

  // Time block
  const float timeX = x0 + (PANEL_W - TIME_W) * 0.5f;
  const float timeY = y0 + (PANEL_H - TIME_H) * 0.5f;

  m_timeToggleBtn.setSize({TOGGLE_W, TOGGLE_H});
  m_timeToggleBtn.setPosition(snap({x0 + (PANEL_W - TOGGLE_W) * 0.5f, timeY - 56.f}));
  m_timeToggleBtn.setOutlineThickness(0.f);
  m_timeToggleText.setFont(m_font);
  m_timeToggleText.setCharacterSize(16);

  m_timePanel.setSize({TIME_W, TIME_H});
  m_timePanel.setPosition(snap({timeX, timeY}));
  m_timePanel.setFillColor(ColorPaletteManager::get().palette().COL_HEADER);
  m_timePanel.setOutlineThickness(1.f);
  m_timePanel.setOutlineColor(colPanelBorder);

  m_timeTitle.setFont(m_font);
  m_timeTitle.setCharacterSize(14);
  m_timeTitle.setFillColor(colSubtle);
  m_timeTitle.setString("Time Control");
  m_timeTitle.setPosition(snap({timeX + 10.f, timeY + 8.f}));

  m_timeMain.setFont(m_font);
  m_timeMain.setCharacterSize(22);
  m_timeMain.setFillColor(colText);
  m_timeMain.setString(formatHMS(m_baseSeconds));

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

  // Presets
  m_presets.clear();
  auto makeChip = [&](const char* label, int base, int inc) {
    PresetChip c;
    float chipW = 74.f;
    c.box.setSize({chipW, CHIP_H});
    c.box.setFillColor(colButton);
    c.box.setOutlineThickness(0.f);
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

  auto layoutTimeControls = [&]() {
    const sf::Vector2f p = m_timePanel.getPosition();
    float row1Y = p.y + 42.f;
    const float gap = 10.f, mw = m_timeMinusBtn.getSize().x, pw = m_timePlusBtn.getSize().x;
    auto mb = m_timeMain.getLocalBounds();
    float totalW = mw + gap + mb.width + gap + pw;
    float left = p.x + (TIME_W - totalW) * 0.5f;

    m_timeMinusBtn.setPosition(snap({left, row1Y - m_timeMinusBtn.getSize().y * 0.5f}));
    m_timePlusBtn.setPosition(
        snap({left + mw + gap + mb.width + gap, row1Y - m_timePlusBtn.getSize().y * 0.5f}));

    sf::FloatRect minusGB = m_timeMinusBtn.getGlobalBounds();
    sf::FloatRect midBox(minusGB.left + minusGB.width + gap, row1Y - 14.f, mb.width, 28.f);
    centerText(m_timeMain, midBox);
    centerText(m_minusTxt, m_timeMinusBtn.getGlobalBounds());
    centerText(m_plusTxt, m_timePlusBtn.getGlobalBounds());

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

    float yChips = p.y + TIME_H + 12.f;
    float chipsTotalW = 3.f * m_presets[0].box.getSize().x + 2.f * CHIP_GAP;
    float chipsLeft = p.x + (TIME_W - chipsTotalW) * 0.5f;
    for (std::size_t i = 0; i < m_presets.size(); ++i) {
      float x = chipsLeft + i * (m_presets[i].box.getSize().x + CHIP_GAP);
      m_presets[i].box.setPosition(snap({x, yChips}));
      centerText(m_presets[i].label, m_presets[i].box.getGlobalBounds(), -1.f);
    }
  };

  m_timeMain.setString(formatHMS(m_baseSeconds));
  m_incValue.setString("+" + std::to_string(m_incrementSeconds) + "s");
  updateTimeToggle();
  layoutTimeControls();
}

void StartScreen::applyTheme() {
  m_paletteText.setFillColor(colText);
  m_paletteButton.setFillColor(colButton);
  for (auto& opt : m_paletteOptions) {
    opt.box.setFillColor(colButton);
    opt.label.setFillColor(colText);
  }

  m_whiteLabel.setFillColor(colText);
  m_blackLabel.setFillColor(colText);

  m_whitePlayerBtn.setFillColor(colButton);
  m_whiteBotBtn.setFillColor(colButton);
  m_whitePlayerText.setFillColor(colText);
  m_whiteBotText.setFillColor(colText);
  m_blackPlayerBtn.setFillColor(colButton);
  m_blackBotBtn.setFillColor(colButton);
  m_blackPlayerText.setFillColor(colText);
  m_blackBotText.setFillColor(colText);

  m_startBtn.setFillColor(colAccent);
  m_startText.setFillColor(constant::COL_DARK_TEXT);

  m_loadGameBtn.setFillColor(colButton);
  m_loadGameText.setFillColor(colText);

  m_fenErrorText.setFillColor(colInvalid);
  m_fenInputBox.setFillColor(colInput);
  m_fenInputBox.setOutlineColor(colInputBorder);
  m_fenInputText.setFillColor(colText);
  m_pgnInputBox.setFillColor(colInput);
  m_pgnInputBox.setOutlineColor(colInputBorder);
  m_pgnInputText.setFillColor(colText);
  m_pgnErrorText.setFillColor(colInvalid);

  m_modalPanel.setFillColor(colTextPanel);
  m_modalPanel.setOutlineColor(colPanelBorder);
  m_modalPanel.setOutlineThickness(1.f);
  m_modalTitle.setFillColor(colText);
  m_modalSubtitle.setFillColor(colSubtle);
  m_modalFenLabel.setFillColor(colSubtle);
  m_modalPgnLabel.setFillColor(colSubtle);
  m_modalCancelBtn.setFillColor(colButton);
  m_modalCancelText.setFillColor(colText);
  m_modalApplyBtn.setFillColor(colAccent);
  m_modalApplyText.setFillColor(constant::COL_DARK_TEXT);

  for (auto& opt : m_whiteBotOptions) {
    opt.box.setFillColor(colButton);
    opt.label.setFillColor(colText);
  }
  for (auto& opt : m_blackBotOptions) {
    opt.box.setFillColor(colButton);
    opt.label.setFillColor(colText);
  }

  m_timePanel.setFillColor(ColorPaletteManager::get().palette().COL_HEADER);
  m_timePanel.setOutlineColor(colPanelBorder);
  m_timeTitle.setFillColor(colSubtle);
  m_timeMain.setFillColor(colText);
  m_timeMinusBtn.setFillColor(colButton);
  m_timePlusBtn.setFillColor(colButton);
  m_minusTxt.setFillColor(colText);
  m_incLabel.setFillColor(colSubtle);
  m_incValue.setFillColor(colText);
  m_incMinusBtn.setFillColor(colButton);
  m_incPlusBtn.setFillColor(colButton);
  m_incMinusTxt.setFillColor(colText);

  for (auto& c : m_presets) {
    c.box.setFillColor(colButton);
    c.label.setFillColor(colText);
  }

  updateTimeToggle();
}

void StartScreen::updateTimeToggle() {
  if (m_timeEnabled) {
    m_timeToggleBtn.setFillColor(colAccent);
    m_timeToggleText.setFillColor(constant::COL_DARK_TEXT);
    m_timeToggleText.setString("TIME ON");
  } else {
    m_timeToggleBtn.setFillColor(colTimeOff);
    m_timeToggleText.setFillColor(colText);
    m_timeToggleText.setString("TIME OFF");
  }
  centerText(m_timeToggleText, m_timeToggleBtn.getGlobalBounds());
}

static void drawPanelWithShadow(sf::RenderWindow& win, const sf::Vector2f& topLeft) {
  // Keep panel shadowed (overall), not the clock panel
  sf::FloatRect rect(topLeft.x, topLeft.y, PANEL_W, PANEL_H);
  // soft shadow
  for (int i = 3; i >= 1; --i) {
    float grow = (float)i * 6.f;
    sf::RectangleShape s({rect.width + 2.f * grow, rect.height + 2.f * grow});
    s.setPosition(snapf(rect.left - grow), snapf(rect.top - grow));
    sf::Color sc(0, 0, 0, (sf::Uint8)(30 * i));
    s.setFillColor(sc);
    win.draw(s);
  }
  // border + body
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
  if (m_showPaletteList) {
    for (std::size_t i = 0; i < m_paletteOptions.size(); ++i) {
      auto& opt = m_paletteOptions[i];
      if (contains(opt.box.getGlobalBounds(), pos)) {
        ColorPaletteManager::get().setPalette(opt.name);
        setupUI();
        m_showPaletteList = false;
        m_paletteListForceHide = true;
        return false;
      }
    }
  }

  // White toggles
  if (contains(m_whitePlayerBtn.getGlobalBounds(), pos)) {
    cfg.whiteIsBot = false;
    return false;
  }
  if (contains(m_whiteBotBtn.getGlobalBounds(), pos)) {
    cfg.whiteIsBot = true;
    return false;
  }
  if (m_showWhiteBotList) {
    for (std::size_t i = 0; i < m_whiteBotOptions.size(); ++i) {
      if (contains(m_whiteBotOptions[i].box.getGlobalBounds(), pos)) {
        m_whiteBotSelection = i;
        cfg.whiteBot = m_whiteBotOptions[i].type;
        cfg.whiteIsBot = true;
        m_whiteBotText.setString(botDisplayName(cfg.whiteBot));
        m_showWhiteBotList = false;
        m_whiteListForceHide = true;
        return false;
      }
    }
  }

  // Black toggles
  if (contains(m_blackPlayerBtn.getGlobalBounds(), pos)) {
    cfg.blackIsBot = false;
    return false;
  }
  if (contains(m_blackBotBtn.getGlobalBounds(), pos)) {
    cfg.blackIsBot = true;
    return false;
  }
  if (m_showBlackBotList) {
    for (std::size_t i = 0; i < m_blackBotOptions.size(); ++i) {
      if (contains(m_blackBotOptions[i].box.getGlobalBounds(), pos)) {
        m_blackBotSelection = i;
        cfg.blackBot = m_blackBotOptions[i].type;
        cfg.blackIsBot = true;
        m_blackBotText.setString(botDisplayName(cfg.blackBot));
        m_showBlackBotList = false;
        m_blackListForceHide = true;
        return false;
      }
    }
  }

  // Time presets (if enabled)
  if (m_timeEnabled) {
    for (std::size_t i = 0; i < m_presets.size(); ++i) {
      auto& chip = m_presets[i];
      if (contains(chip.box.getGlobalBounds(), pos)) {
        m_presetSelection = (int)i;
        m_baseSeconds = clampBaseSeconds(chip.base);
        m_incrementSeconds = clampIncSeconds(chip.inc);
        m_timeMain.setString(formatHMS(m_baseSeconds));
        m_incValue.setString("+" + std::to_string(m_incrementSeconds) + "s");
        return false;
      }
    }
  }

  if (contains(m_loadGameBtn.getGlobalBounds(), pos)) {
    m_showLoadModal = true;
    return false;
  }

  // Start
  if (contains(m_startBtn.getGlobalBounds(), pos)) return true;

  return false;
}

bool StartScreen::isValidFen(const std::string& fen) {
  return basicFenCheck(fen);
}

bool StartScreen::isValidPgn(const std::string& pgn) {
  return basicPgnCheck(pgn);
}

void StartScreen::processHoldRepeater(HoldRepeater& r, const sf::FloatRect& bounds,
                                      sf::Vector2f mouse, std::function<void()> stepFn,
                                      float initialDelay, float repeatRate) {
  if (!r.active) return;
  if (!bounds.contains(mouse)) return;
  float t = r.clock.getElapsedTime().asSeconds();
  if (t < initialDelay) return;
  int ticks = (int)((t - initialDelay) / repeatRate);
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
  cfg.fen = core::START_FEN;
  cfg.pgn.clear();
  cfg.timeBaseSeconds = m_baseSeconds;
  cfg.timeIncrementSeconds = m_incrementSeconds;
  cfg.timeEnabled = m_timeEnabled;

  enum class LoadField { None, Fen, Pgn };
  LoadField activeField = LoadField::None;

  bool fenErrorActive = false;
  bool pgnErrorActive = false;

  bool toastVisible = false;
  sf::Clock toastClock;
  std::string toastMsg;

  sf::Clock caretClock;
  sf::Clock frameClock;

  auto resetCaret = [&]() {
    caretClock.restart();
  };

  auto closeModal = [&]() {
    m_showLoadModal = false;
    activeField = LoadField::None;
    fenErrorActive = false;
    pgnErrorActive = false;
    resetCaret();
  };

  auto drawLoadModal = [&]() {
    const bool fenHasText = !m_fenString.empty();
    const bool fenValid = !fenHasText || isValidFen(m_fenString);
    const bool showFenError = fenErrorActive && !fenValid;

    const bool pgnHasText = !m_pgnString.empty();
    const bool pgnValid = !pgnHasText || isValidPgn(m_pgnString);
    const bool showPgnError = pgnErrorActive && !pgnValid;

    m_window.draw(m_modalBackdrop);

    sf::RectangleShape shadow(m_modalPanel.getSize() + sf::Vector2f(12.f, 12.f));
    shadow.setPosition(m_modalPanel.getPosition() - sf::Vector2f(6.f, 6.f));
    shadow.setFillColor(sf::Color(0, 0, 0, 120));
    m_window.draw(shadow);

    m_window.draw(m_modalPanel);

    m_window.draw(m_modalTitle);
    m_window.draw(m_modalSubtitle);
    m_window.draw(m_modalFenLabel);
    m_window.draw(m_modalPgnLabel);

    sf::Color fenOutline = colInputBorder;
    if (fenHasText) fenOutline = fenValid ? colValid : colInvalid;
    if (activeField == LoadField::Fen) fenOutline = colAccent;
    if (fenHasText && !fenValid) fenOutline = colInvalid;
    m_fenInputBox.setOutlineColor(fenOutline);
    m_window.draw(m_fenInputBox);

    if (fenHasText) {
      sf::Text fenText(m_fenString, m_font, 16);
      auto fb = fenText.getLocalBounds();
      fenText.setOrigin(fb.left, fb.top + fb.height / 2.f);
      fenText.setPosition(m_fenInputBox.getPosition().x + 10.f,
                          m_fenInputBox.getPosition().y + m_fenInputBox.getSize().y / 2.f);
      fenText.setFillColor(colText);
      m_window.draw(fenText);
    } else {
      sf::Text placeholder("STANDARD FEN", m_font, 16);
      auto pb = placeholder.getLocalBounds();
      placeholder.setOrigin(pb.left, pb.top + pb.height / 2.f);
      placeholder.setPosition(m_fenInputBox.getPosition().x + 10.f,
                              m_fenInputBox.getPosition().y + m_fenInputBox.getSize().y / 2.f);
      placeholder.setFillColor(colSubtle);
      m_window.draw(placeholder);
    }

    if (activeField == LoadField::Fen) {
      float t = std::fmod(caretClock.getElapsedTime().asSeconds(), 1.0f);
      if (t < 0.5f) {
        sf::Text fenText(m_fenString, m_font, 16);
        fenText.setPosition(m_fenInputBox.getPosition().x + 10.f,
                            m_fenInputBox.getPosition().y + m_fenInputBox.getSize().y / 2.f);
        auto fb = fenText.getLocalBounds();
        fenText.setOrigin(fb.left, fb.top + fb.height / 2.f);
        auto caretPos = fenText.findCharacterPos(fenText.getString().getSize());
        sf::RectangleShape caret({2.f, m_fenInputBox.getSize().y * 0.6f});
        caret.setPosition(snapf(caretPos.x + 1.f),
                          snapf(m_fenInputBox.getPosition().y + (m_fenInputBox.getSize().y - caret.getSize().y) * 0.5f));
        caret.setFillColor(colText);
        m_window.draw(caret);
      }
    }

    if (showFenError) {
      m_fenErrorText.setString("Invalid FEN format");
      m_window.draw(m_fenErrorText);
    }

    sf::Color pgnOutline = colInputBorder;
    if (pgnHasText) pgnOutline = pgnValid ? colValid : colInvalid;
    if (activeField == LoadField::Pgn) pgnOutline = colAccent;
    if (pgnHasText && !pgnValid) pgnOutline = colInvalid;
    m_pgnInputBox.setOutlineColor(pgnOutline);
    m_window.draw(m_pgnInputBox);

    if (pgnHasText) {
      sf::Text pgnText(m_pgnString, m_font, 15);
      pgnText.setLineSpacing(1.2f);
      auto pb = pgnText.getLocalBounds();
      pgnText.setOrigin(pb.left, pb.top);
      pgnText.setPosition(m_pgnInputBox.getPosition().x + 10.f,
                          m_pgnInputBox.getPosition().y + 10.f);
      pgnText.setFillColor(colText);
      m_window.draw(pgnText);
    } else {
      sf::Text placeholder("Paste PGN notation here", m_font, 15);
      auto pb = placeholder.getLocalBounds();
      placeholder.setOrigin(pb.left, pb.top);
      placeholder.setPosition(m_pgnInputBox.getPosition().x + 10.f,
                              m_pgnInputBox.getPosition().y + 10.f);
      placeholder.setFillColor(colSubtle);
      m_window.draw(placeholder);
    }

    if (activeField == LoadField::Pgn) {
      float t = std::fmod(caretClock.getElapsedTime().asSeconds(), 1.0f);
      if (t < 0.5f) {
        sf::Text pgnText(m_pgnString, m_font, 15);
        pgnText.setLineSpacing(1.2f);
        pgnText.setPosition(m_pgnInputBox.getPosition().x + 10.f,
                            m_pgnInputBox.getPosition().y + 10.f);
        auto pb = pgnText.getLocalBounds();
        pgnText.setOrigin(pb.left, pb.top);
        auto caretPos = pgnText.findCharacterPos(pgnText.getString().getSize());
        sf::RectangleShape caret({2.f, 18.f});
        caret.setPosition(snapf(caretPos.x + 1.f), caretPos.y);
        caret.setFillColor(colText);
        m_window.draw(caret);
      }
    }

    if (showPgnError) {
      m_pgnErrorText.setString("Invalid PGN notation");
      m_window.draw(m_pgnErrorText);
    }

    auto drawModalButton = [&](sf::RectangleShape& button, sf::Text& text, sf::Color base,
                               bool hovered, bool pressed) {
      drawBevelButton3D(m_window, button.getGlobalBounds(), base, hovered, pressed);
      centerText(text, button.getGlobalBounds());
      m_window.draw(text);
    };

    bool cancelHover = contains(m_modalCancelBtn.getGlobalBounds(), m_mousePos);
    bool applyHover = contains(m_modalApplyBtn.getGlobalBounds(), m_mousePos);
    drawModalButton(m_modalCancelBtn, m_modalCancelText, colButton, cancelHover, false);
    drawModalButton(m_modalApplyBtn, m_modalApplyText, colAccent, applyHover, false);
  };

  auto drawUI = [&]() {
    drawVerticalGradient(m_window, colBGTop, colBGBottom);

    bool palHover = (!m_showLoadModal &&
                     (contains(m_paletteButton.getGlobalBounds(), m_mousePos) || m_showPaletteList ||
                      m_paletteListAnim > 0.f));
    m_paletteButton.setFillColor(palHover ? colButtonActive : colButton);
    m_window.draw(m_paletteButton);
    m_window.draw(m_paletteText);
    if (!m_showLoadModal && m_paletteListAnim > 0.f) {
      for (std::size_t i = 0; i < m_paletteOptions.size(); ++i) {
        const auto& opt = m_paletteOptions[i];
        auto r = opt.box.getGlobalBounds();
        bool hov = contains(r, m_mousePos);
        bool sel = (i == m_paletteSelection);
        sf::Color base = sel ? colButtonActive : colButton;
        base.a = static_cast<sf::Uint8>(base.a * m_paletteListAnim);
        drawBevelButton3D(m_window, r, base, hov, sel);
        sf::Text label = opt.label;
        sf::Color lc = label.getFillColor();
        lc.a = static_cast<sf::Uint8>(lc.a * m_paletteListAnim);
        label.setFillColor(lc);
        leftCenterText(label, r, 8.f);
        m_window.draw(label);
        if (sel) {
          sf::Color ac = colAccent;
          ac.a = static_cast<sf::Uint8>(ac.a * m_paletteListAnim);
          drawAccentInset(m_window, r, ac);
        }
      }
    }

    if (m_logoTex.getSize().x > 0 && m_logoTex.getSize().y > 0) {
      sf::Sprite logoBG(m_logoTex);
      const auto ws = m_window.getSize();
      const float desiredH = ws.y * 0.90f;
      const float s = desiredH / (float)m_logoTex.getSize().y;
      logoBG.setScale(s, s);
      auto lb = logoBG.getLocalBounds();
      logoBG.setOrigin(lb.width, 0.f);
      logoBG.setPosition(snapf((float)ws.x - 24.f), snapf(24.f));
      logoBG.setColor(ColorPaletteManager::get().palette().COL_LOGO_BG);
      m_window.draw(logoBG, sf::RenderStates(sf::BlendAlpha));
    }

    sf::Vector2f panelPos((m_window.getSize().x - PANEL_W) * 0.5f,
                          (m_window.getSize().y - PANEL_H) * 0.5f);
    drawPanelWithShadow(m_window, panelPos);

    sf::Text title("Lilia Engine - Bot Sandbox", m_font, 28);
    title.setFillColor(colText);
    title.setPosition(snapf(panelPos.x + 24.f), snapf(panelPos.y + 18.f));
    m_window.draw(title);

    sf::Text subtitle("Try different chess bots. Choose sides & engine.", m_font, 18);
    subtitle.setFillColor(colSubtle);
    subtitle.setPosition(snapf(panelPos.x + 24.f), snapf(panelPos.y + 52.f));
    m_window.draw(subtitle);

    m_window.draw(m_whiteLabel);
    m_window.draw(m_blackLabel);

    auto drawColumn = [&](sf::RectangleShape& humanBtn, sf::Text& humanTxt, bool humanActive,
                          sf::RectangleShape& botBtn, sf::Text& botTxt, bool botActive,
                          bool listVisible) {
      auto humanR = humanBtn.getGlobalBounds();
      auto botR = botBtn.getGlobalBounds();
      bool hovH = !m_showLoadModal && contains(humanR, m_mousePos);
      bool hovB = !m_showLoadModal && contains(botR, m_mousePos);
      drawBevelButton3D(m_window, humanR, humanActive ? colButtonActive : colButton, hovH, humanActive);
      centerText(humanTxt, humanR);
      m_window.draw(humanTxt);
      if (humanActive) drawAccentInset(m_window, humanR, colAccent);
      drawBevelButton3D(m_window, botR, botActive ? colButtonActive : colButton, hovB || listVisible,
                       botActive);
      centerText(botTxt, botR);
      m_window.draw(botTxt);
      if (botActive) drawAccentInset(m_window, botR, colAccent);
    };

    drawColumn(m_whitePlayerBtn, m_whitePlayerText, !cfg.whiteIsBot, m_whiteBotBtn, m_whiteBotText,
               cfg.whiteIsBot, m_showWhiteBotList);
    drawColumn(m_blackPlayerBtn, m_blackPlayerText, !cfg.blackIsBot, m_blackBotBtn, m_blackBotText,
               cfg.blackIsBot, m_showBlackBotList);

    auto drawBotList = [&](const std::vector<BotOption>& list, std::size_t selIdx, float anim) {
      for (std::size_t i = 0; i < list.size(); ++i) {
        const auto& opt = list[i];
        auto r = opt.box.getGlobalBounds();
        bool hov = contains(r, m_mousePos);
        bool sel = (i == selIdx);
        sf::Color base = sel ? colButtonActive : colButton;
        base.a = static_cast<sf::Uint8>(base.a * anim);
        drawBevelButton3D(m_window, r, base, hov, sel);
        sf::Text label = opt.label;
        sf::Color lc = label.getFillColor();
        lc.a = static_cast<sf::Uint8>(lc.a * anim);
        label.setFillColor(lc);
        leftCenterText(label, r, 10.f);
        m_window.draw(label);
        if (sel) {
          sf::Color ac = colAccent;
          ac.a = static_cast<sf::Uint8>(ac.a * anim);
          drawAccentInset(m_window, r, ac);
        }
      }
    };
    if (!m_showLoadModal && m_whiteBotListAnim > 0.f)
      drawBotList(m_whiteBotOptions, m_whiteBotSelection, m_whiteBotListAnim);
    if (!m_showLoadModal && m_blackBotListAnim > 0.f)
      drawBotList(m_blackBotOptions, m_blackBotSelection, m_blackBotListAnim);

    {
      auto gb = m_timeToggleBtn.getGlobalBounds();
      bool hov = !m_showLoadModal && contains(gb, m_mousePos);
      bool on = m_timeEnabled;
      sf::Color base = on ? colAccent : colTimeOff;
      drawBevelButton3D(m_window, gb, base, hov, on);
      centerText(m_timeToggleText, gb);
      m_window.draw(m_timeToggleText);
    }

    if (m_timeEnabled) {
      auto gb = m_timePanel.getGlobalBounds();
      m_window.draw(m_timePanel);
      sf::RectangleShape top({gb.width, 1.f});
      top.setPosition(gb.left, gb.top);
      top.setFillColor(ColorPaletteManager::get().palette().COL_TOP_HILIGHT);
      m_window.draw(top);
      sf::RectangleShape bot({gb.width, 1.f});
      bot.setPosition(gb.left, gb.top + gb.height - 1.f);
      bot.setFillColor(ColorPaletteManager::get().palette().COL_BOTTOM_SHADOW);
      m_window.draw(bot);

      auto stepBtn = [&](sf::RectangleShape& box, sf::Text& txt, bool hold) {
        auto r = box.getGlobalBounds();
        bool hov = !m_showLoadModal && contains(r, m_mousePos);
        bool pressed = hold && hov;
        drawBevelButton3D(m_window, r, colButton, hov, pressed);
        centerText(txt, r);
        m_window.draw(txt);
      };
      stepBtn(m_timeMinusBtn, m_minusTxt, m_holdBaseMinus.active);
      stepBtn(m_timePlusBtn, m_plusTxt, m_holdBasePlus.active);
      m_window.draw(m_timeMain);

      m_window.draw(m_incLabel);
      stepBtn(m_incMinusBtn, m_incMinusTxt, m_holdIncMinus.active);
      stepBtn(m_incPlusBtn, m_incPlusTxt, m_holdIncPlus.active);
      m_window.draw(m_incValue);

      for (std::size_t i = 0; i < m_presets.size(); ++i) {
        auto& c = m_presets[i];
        auto r = c.box.getGlobalBounds();
        bool hov = !m_showLoadModal && contains(r, m_mousePos);
        bool sel = (m_presetSelection == (int)i);
        drawBevelButton3D(m_window, r, sel ? colButtonActive : colButton, hov, sel);
        centerText(c.label, r, -1.f);
        m_window.draw(c.label);
        if (sel) drawAccentInset(m_window, r, colAccent);
      }
    }

    {
      auto r = m_startBtn.getGlobalBounds();
      bool hov = !m_showLoadModal && contains(r, m_mousePos);
      drawBevelButton3D(m_window, r, colAccent, hov, false);
      centerText(m_startText, r);
      m_window.draw(m_startText);
    }

    {
      auto r = m_loadGameBtn.getGlobalBounds();
      bool hov = !m_showLoadModal && contains(r, m_mousePos);
      bool active = m_showLoadModal;
      sf::Color base = active ? colButtonActive : colButton;
      drawBevelButton3D(m_window, r, base, hov, active);
      centerText(m_loadGameText, r);
      m_window.draw(m_loadGameText);
    }

    if (toastVisible) {
      float elapsed = toastClock.getElapsedTime().asSeconds();
      if (elapsed < 2.2f) {
        sf::Text ttxt(toastMsg, m_font, 14);
        ttxt.setFillColor(colText);
        auto tb = ttxt.getLocalBounds();
        float pad = 12.f;
        float bw = tb.width + pad * 2.f;
        float bh = tb.height + pad * 2.f;
        sf::Vector2u ws = m_window.getSize();
        float x = (ws.x - bw) * 0.5f;
        float y = ws.y - bh - 24.f;
        sf::RectangleShape bg({bw, bh});
        bg.setPosition(snapf(x), snapf(y));
        bg.setFillColor(ColorPaletteManager::get().palette().COL_PANEL_ALPHA220);
        bg.setOutlineThickness(1.f);
        bg.setOutlineColor(colPanelBorder);
        m_window.draw(bg);
        ttxt.setPosition(snapf(x + pad - tb.left), snapf(y + pad - tb.top));
        m_window.draw(ttxt);
      } else {
        toastVisible = false;
      }
    }

    sf::Text credit("@ 2025 Julian Meyer", m_font, 13);
    credit.setFillColor(colSubtle);
    auto cb = credit.getLocalBounds();
    sf::Vector2u ws = m_window.getSize();
    credit.setPosition(snapf((float)ws.x - cb.width - 18.f),
                       snapf((float)ws.y - cb.height - 22.f));
    m_window.draw(credit);

    if (m_showLoadModal) {
      drawLoadModal();
    }
  };

  while (m_window.isOpen()) {
    float dt = frameClock.restart().asSeconds();
    sf::Event e{};
    while (m_window.pollEvent(e)) {
      if (e.type == sf::Event::Closed) {
        m_window.close();
        break;
      }
      if (e.type == sf::Event::Resized) {
        setupUI();
      }
      if (e.type == sf::Event::MouseMoved) {
        m_mousePos = {(float)e.mouseMove.x, (float)e.mouseMove.y};
        if (!m_showLoadModal) {
          auto updateHover = [&](bool& show, bool& forceHide, const sf::FloatRect& btn,
                                 const auto& options) {
            bool overBtn = contains(btn, m_mousePos);
            bool overList = false;
            for (const auto& opt : options) {
              if (contains(opt.box.getGlobalBounds(), m_mousePos)) {
                overList = true;
                break;
              }
            }
            if (!overList) forceHide = false;
            show = !forceHide && (overBtn || overList);
          };
          updateHover(m_showPaletteList, m_paletteListForceHide, m_paletteButton.getGlobalBounds(),
                      m_paletteOptions);
          updateHover(m_showWhiteBotList, m_whiteListForceHide, m_whiteBotBtn.getGlobalBounds(),
                      m_whiteBotOptions);
          updateHover(m_showBlackBotList, m_blackListForceHide, m_blackBotBtn.getGlobalBounds(),
                      m_blackBotOptions);
        }
      }

      if (m_showLoadModal) {
        if (e.type == sf::Event::KeyPressed) {
          if (e.key.code == sf::Keyboard::Escape) {
            closeModal();
          } else if (e.key.code == sf::Keyboard::Enter) {
            bool fenOk = m_fenString.empty() || isValidFen(m_fenString);
            bool pgnOk = m_pgnString.empty() || isValidPgn(m_pgnString);
            fenErrorActive = !fenOk && !m_fenString.empty();
            pgnErrorActive = !pgnOk && !m_pgnString.empty();
            if (fenOk && pgnOk) closeModal();
          } else if ((e.key.control || e.key.system) && e.key.code == sf::Keyboard::V) {
            auto clip = sf::Clipboard::getString().toAnsiString();
            if (activeField == LoadField::Fen) {
              std::string cleaned = clip;
              cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '\n'), cleaned.end());
              cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '\r'), cleaned.end());
              const float avail = m_fenInputBox.getSize().x - 20.f;
              std::string out = m_fenString;
              for (char c : cleaned) {
                if (c < 32) continue;
                sf::Text probe(out + c, m_font, 16);
                if (probe.getLocalBounds().width <= avail)
                  out.push_back(c);
                else
                  break;
              }
              m_fenString = out;
            } else if (activeField == LoadField::Pgn) {
              m_pgnString += clip;
            }
          }
        }

        if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
          sf::Vector2f mp((float)e.mouseButton.x, (float)e.mouseButton.y);
          if (contains(m_modalApplyBtn.getGlobalBounds(), mp)) {
            bool fenOk = m_fenString.empty() || isValidFen(m_fenString);
            bool pgnOk = m_pgnString.empty() || isValidPgn(m_pgnString);
            fenErrorActive = !fenOk && !m_fenString.empty();
            pgnErrorActive = !pgnOk && !m_pgnString.empty();
            if (fenOk && pgnOk) closeModal();
          } else if (contains(m_modalCancelBtn.getGlobalBounds(), mp)) {
            closeModal();
          } else if (contains(m_fenInputBox.getGlobalBounds(), mp)) {
            activeField = LoadField::Fen;
            resetCaret();
          } else if (contains(m_pgnInputBox.getGlobalBounds(), mp)) {
            activeField = LoadField::Pgn;
            resetCaret();
          }
        }

        if (e.type == sf::Event::TextEntered) {
          if (activeField == LoadField::Fen) {
            if (e.text.unicode == 8) {
              if (!m_fenString.empty()) m_fenString.pop_back();
            } else if (e.text.unicode >= 32 && e.text.unicode < 127) {
              const float avail = m_fenInputBox.getSize().x - 20.f;
              std::string tmp = m_fenString;
              tmp.push_back(static_cast<char>(e.text.unicode));
              sf::Text probe(tmp, m_font, 16);
              if (probe.getLocalBounds().width <= avail) {
                m_fenString.push_back(static_cast<char>(e.text.unicode));
              }
            }
          } else if (activeField == LoadField::Pgn) {
            if (e.text.unicode == 8) {
              if (!m_pgnString.empty()) m_pgnString.pop_back();
            } else if (e.text.unicode == '\r') {
              m_pgnString.push_back('\n');
            } else if (e.text.unicode >= 32) {
              m_pgnString.push_back(static_cast<char>(e.text.unicode));
            }
          }
        }
        continue;
      }

      if (e.type == sf::Event::KeyPressed) {
        if (m_timeEnabled && e.key.code == sf::Keyboard::Left) {
          m_baseSeconds = clampBaseSeconds(m_baseSeconds - (e.key.shift ? 300 : 60));
          m_timeMain.setString(formatHMS(m_baseSeconds));
        } else if (m_timeEnabled && e.key.code == sf::Keyboard::Right) {
          m_baseSeconds = clampBaseSeconds(m_baseSeconds + (e.key.shift ? 300 : 60));
          m_timeMain.setString(formatHMS(m_baseSeconds));
        } else if (m_timeEnabled && e.key.code == sf::Keyboard::Down) {
          m_incrementSeconds = clampIncSeconds(m_incrementSeconds - 1);
          m_incValue.setString("+" + std::to_string(m_incrementSeconds) + "s");
        } else if (m_timeEnabled && e.key.code == sf::Keyboard::Up) {
          m_incrementSeconds = clampIncSeconds(m_incrementSeconds + 1);
          m_incValue.setString("+" + std::to_string(m_incrementSeconds) + "s");
        } else if (e.key.code == sf::Keyboard::Enter) {
          bool fenOk = m_fenString.empty() || isValidFen(m_fenString);
          bool pgnOk = m_pgnString.empty() || isValidPgn(m_pgnString);
          if (!fenOk && !m_fenString.empty()) {
            toastMsg = "Invalid FEN. Using standard.";
            toastVisible = true;
            toastClock.restart();
          }
          if (!pgnOk && !m_pgnString.empty()) {
            toastMsg = "Invalid PGN ignored.";
            toastVisible = true;
            toastClock.restart();
          }
          cfg.timeBaseSeconds = m_baseSeconds;
          cfg.timeIncrementSeconds = m_incrementSeconds;
          cfg.timeEnabled = m_timeEnabled;
          cfg.fen = fenOk && !m_fenString.empty() ? m_fenString : core::START_FEN;
          cfg.pgn = pgnOk ? m_pgnString : std::string();
          return cfg;
        }
      }

      if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2f mp((float)e.mouseButton.x, (float)e.mouseButton.y);
        if (m_timeEnabled && contains(m_timeMinusBtn.getGlobalBounds(), mp)) {
          m_baseSeconds = clampBaseSeconds(m_baseSeconds - 60);
          m_timeMain.setString(formatHMS(m_baseSeconds));
          m_holdBaseMinus.active = true;
          m_holdBaseMinus.clock.restart();
          m_holdBaseMinus.fired = 0;
        } else if (m_timeEnabled && contains(m_timePlusBtn.getGlobalBounds(), mp)) {
          m_baseSeconds = clampBaseSeconds(m_baseSeconds + 60);
          m_timeMain.setString(formatHMS(m_baseSeconds));
          m_holdBasePlus.active = true;
          m_holdBasePlus.clock.restart();
          m_holdBasePlus.fired = 0;
        } else if (m_timeEnabled && contains(m_incMinusBtn.getGlobalBounds(), mp)) {
          m_incrementSeconds = clampIncSeconds(m_incrementSeconds - 1);
          m_incValue.setString("+" + std::to_string(m_incrementSeconds) + "s");
          m_holdIncMinus.active = true;
          m_holdIncMinus.clock.restart();
          m_holdIncMinus.fired = 0;
        } else if (m_timeEnabled && contains(m_incPlusBtn.getGlobalBounds(), mp)) {
          m_incrementSeconds = clampIncSeconds(m_incrementSeconds + 1);
          m_incValue.setString("+" + std::to_string(m_incrementSeconds) + "s");
          m_holdIncPlus.active = true;
          m_holdIncPlus.clock.restart();
          m_holdIncPlus.fired = 0;
        } else if (contains(m_timeToggleBtn.getGlobalBounds(), mp)) {
          m_timeEnabled = !m_timeEnabled;
          updateTimeToggle();
          m_holdBaseMinus.active = m_holdBasePlus.active = m_holdIncMinus.active =
              m_holdIncPlus.active = false;
        } else {
          bool modalBefore = m_showLoadModal;
          if (handleMouse(mp, cfg)) {
            bool fenOk = m_fenString.empty() || isValidFen(m_fenString);
            bool pgnOk = m_pgnString.empty() || isValidPgn(m_pgnString);
            if (!fenOk && !m_fenString.empty()) {
              toastMsg = "Invalid FEN. Using standard.";
              toastVisible = true;
              toastClock.restart();
            }
            if (!pgnOk && !m_pgnString.empty()) {
              toastMsg = "Invalid PGN ignored.";
              toastVisible = true;
              toastClock.restart();
            }
            cfg.timeBaseSeconds = m_baseSeconds;
            cfg.timeIncrementSeconds = m_incrementSeconds;
            cfg.timeEnabled = m_timeEnabled;
            cfg.fen = fenOk && !m_fenString.empty() ? m_fenString : core::START_FEN;
            cfg.pgn = pgnOk ? m_pgnString : std::string();
            if (m_showLoadModal) closeModal();
            if (!m_showLoadModal) return cfg;
          } else if (!modalBefore && m_showLoadModal) {
            activeField = LoadField::Fen;
            fenErrorActive = pgnErrorActive = false;
            resetCaret();
            m_showPaletteList = m_showWhiteBotList = m_showBlackBotList = false;
            m_paletteListForceHide = m_whiteListForceHide = m_blackListForceHide = true;
          }
        }
      }

      if (e.type == sf::Event::MouseButtonReleased && e.mouseButton.button == sf::Mouse::Left) {
        m_holdBaseMinus.active = m_holdBasePlus.active = m_holdIncMinus.active =
            m_holdIncPlus.active = false;
      }
    }

    if (!m_showLoadModal && m_timeEnabled) {
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
    }

    auto animateList = [&](bool show, float& anim) {
      const float speed = 10.f;
      if (show)
        anim = std::min(1.f, anim + speed * dt);
      else
        anim = std::max(0.f, anim - speed * dt);
    };
    if (!m_showLoadModal) {
      animateList(m_showPaletteList, m_paletteListAnim);
      animateList(m_showWhiteBotList, m_whiteBotListAnim);
      animateList(m_showBlackBotList, m_blackBotListAnim);
    } else {
      m_paletteListAnim = m_whiteBotListAnim = m_blackBotListAnim = 0.f;
    }

    m_window.clear();
    drawUI();
    m_window.display();
  }

  cfg.timeBaseSeconds = m_baseSeconds;
  cfg.timeIncrementSeconds = m_incrementSeconds;
  cfg.timeEnabled = m_timeEnabled;
  cfg.fen = m_fenString.empty() || !isValidFen(m_fenString) ? core::START_FEN : m_fenString;
  cfg.pgn = isValidPgn(m_pgnString) ? m_pgnString : std::string();
  return cfg;
}

}  // namespace lilia::view
