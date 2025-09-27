#include "lilia/view/start_screen.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Window/Clipboard.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <sstream>
#include <unordered_map>

#include "lilia/bot/bot_info.hpp"
#include "lilia/model/chess_game.hpp"
#include "lilia/uci/uci_helper.hpp"
#include "lilia/view/color_palette_manager.hpp"
#include "lilia/view/render_constants.hpp"

namespace lilia::view {

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

// --------- Utils ---------
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

inline sf::Color lighten(sf::Color c, int d) {
  auto clip = [](int x) { return std::clamp(x, 0, 255); };
  return sf::Color(clip(c.r + d), clip(c.g + d), clip(c.b + d), c.a);
}
inline sf::Color darken(sf::Color c, int d) {
  return lighten(c, -d);
}

template <typename T>
bool contains(const sf::Rect<T>& r, sf::Vector2f p) {
  return r.contains(p);
}

// Bots
std::vector<BotType> availableBots() {
  return {BotType::Lilia};
}
std::string botDisplayName(BotType t) {
  return getBotConfig(t).info.name;
}

// FEN validator (basic)
bool basicFenCheck(const std::string& fen) {
  std::istringstream ss(fen);
  std::string fields[6];
  for (int i = 0; i < 6; ++i)
    if (!(ss >> fields[i])) return false;
  std::string extra;
  if (ss >> extra) return false;
  {
    int rankCount = 0, i = 0;
    while (i < (int)fields[0].size()) {
      int fileSum = 0;
      while (i < (int)fields[0].size() && fields[0][i] != '/') {
        char c = fields[0][i++];
        if (std::isdigit((unsigned char)c)) {
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
      if (i < (int)fields[0].size() && fields[0][i] == '/') ++i;
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
      if (!std::isdigit((unsigned char)c)) return false;
    return true;
  };
  if (!isNonNegInt(fields[4])) return false;
  if (!isNonNegInt(fields[5])) return false;
  if (std::stoi(fields[5]) <= 0) return false;
  return true;
}

inline std::string trimCopy(const std::string& in) {
  auto begin = std::find_if_not(in.begin(), in.end(), [](unsigned char c) { return std::isspace(c); });
  auto end = std::find_if_not(in.rbegin(), in.rend(), [](unsigned char c) { return std::isspace(c); }).base();
  if (begin >= end) return {};
  return std::string(begin, end);
}

inline core::Square squareFromStr(const std::string& s) {
  if (s.size() != 2) return core::NO_SQUARE;
  char file = s[0];
  char rank = s[1];
  if (file < 'a' || file > 'h' || rank < '1' || rank > '8') return core::NO_SQUARE;
  int f = file - 'a';
  int r = rank - '1';
  return static_cast<core::Square>(r * 8 + f);
}

inline core::PieceType pieceFromSanChar(char c) {
  switch (c) {
    case 'K':
      return core::PieceType::King;
    case 'Q':
      return core::PieceType::Queen;
    case 'R':
      return core::PieceType::Rook;
    case 'B':
      return core::PieceType::Bishop;
    case 'N':
      return core::PieceType::Knight;
    default:
      return core::PieceType::None;
  }
}

inline char fileChar(core::Square sq) {
  return static_cast<char>('a' + (static_cast<int>(sq) & 7));
}

inline char rankChar(core::Square sq) {
  return static_cast<char>('1' + (static_cast<int>(sq) >> 3));
}

struct ParsedSan {
  bool castleKing{false};
  bool castleQueen{false};
  core::PieceType piece{core::PieceType::Pawn};
  core::Square to{core::NO_SQUARE};
  bool capture{false};
  char fileHint{0};
  char rankHint{0};
  core::PieceType promotion{core::PieceType::None};
};

static bool parseSanToken(const std::string& raw, ParsedSan& out) {
  std::string san;
  san.reserve(raw.size());
  for (char c : raw) {
    if (c == '\r') continue;
    san.push_back(static_cast<char>(c));
  }
  while (!san.empty()) {
    char back = san.back();
    if (back == '+' || back == '#' || back == '!' || back == '?') {
      san.pop_back();
    } else {
      break;
    }
  }
  if (san.empty()) return false;

  auto isCastleToken = [](const std::string& s, const char* pattern) {
    if (s.size() != std::strlen(pattern)) return false;
    for (std::size_t i = 0; i < s.size(); ++i) {
      char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
      char b = static_cast<char>(std::tolower(static_cast<unsigned char>(pattern[i])));
      if (a != b) return false;
    }
    return true;
  };

  if (isCastleToken(san, "o-o") || isCastleToken(san, "0-0")) {
    out = ParsedSan{};
    out.castleKing = true;
    return true;
  }
  if (isCastleToken(san, "o-o-o") || isCastleToken(san, "0-0-0")) {
    out = ParsedSan{};
    out.castleQueen = true;
    return true;
  }

  ParsedSan parsed;

  std::size_t promoPos = san.find('=');
  std::string withoutPromo = san;
  if (promoPos != std::string::npos) {
    if (promoPos + 1 >= san.size()) return false;
    core::PieceType promo = pieceFromSanChar(static_cast<char>(std::toupper(static_cast<unsigned char>(san[promoPos + 1]))));
    if (promo == core::PieceType::None) return false;
    parsed.promotion = promo;
    withoutPromo = san.substr(0, promoPos);
  }

  if (withoutPromo.size() < 2) return false;
  std::string destStr = withoutPromo.substr(withoutPromo.size() - 2);
  parsed.to = squareFromStr(destStr);
  if (parsed.to == core::NO_SQUARE) return false;

  std::string prefix = withoutPromo.substr(0, withoutPromo.size() - 2);
  std::string disamb;
  for (char c : prefix) {
    if (c == 'x' || c == 'X') {
      parsed.capture = true;
      continue;
    }
    disamb.push_back(c);
  }

  if (!disamb.empty() && std::isupper(static_cast<unsigned char>(disamb[0]))) {
    parsed.piece = pieceFromSanChar(disamb[0]);
    if (parsed.piece == core::PieceType::None) return false;
    disamb.erase(disamb.begin());
  }

  for (char c : disamb) {
    if (c >= 'a' && c <= 'h') parsed.fileHint = c;
    else if (c >= '1' && c <= '8') parsed.rankHint = c;
    else
      return false;
  }

  out = parsed;
  return true;
}

static std::vector<std::string> tokenizePgn(const std::string& pgn) {
  std::vector<std::string> tokens;
  std::string current;
  bool inTag = false;
  bool inComment = false;
  bool inLineComment = false;

  auto flush = [&]() {
    if (!current.empty()) {
      tokens.push_back(current);
      current.clear();
    }
  };

  for (char ch : pgn) {
    if (inTag) {
      if (ch == ']') inTag = false;
      continue;
    }
    if (inComment) {
      if (ch == '}') inComment = false;
      continue;
    }
    if (inLineComment) {
      if (ch == '\n') inLineComment = false;
      continue;
    }

    if (ch == '[') {
      flush();
      inTag = true;
      continue;
    }
    if (ch == '{') {
      flush();
      inComment = true;
      continue;
    }
    if (ch == ';') {
      flush();
      inLineComment = true;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(ch))) {
      flush();
      continue;
    }
    current.push_back(ch);
  }
  flush();
  return tokens;
}

static bool isMoveNumberToken(const std::string& tok) {
  if (tok.empty()) return false;
  std::size_t idx = 0;
  while (idx < tok.size() && std::isdigit(static_cast<unsigned char>(tok[idx]))) idx++;
  if (idx == 0) return false;
  for (; idx < tok.size(); ++idx) {
    if (tok[idx] != '.') return false;
  }
  return true;
}

static bool isResultToken(const std::string& tok) {
  return tok == "1-0" || tok == "0-1" || tok == "1/2-1/2" || tok == "*";
}

inline std::string formatHMS(int totalSeconds) {
  totalSeconds = std::max(0, totalSeconds);
  int h = totalSeconds / 3600, m = (totalSeconds % 3600) / 60, s = totalSeconds % 60;
  std::ostringstream ss;
  ss << (h < 10 ? "0" : "") << h << ":" << (m < 10 ? "0" : "") << m << ":" << (s < 10 ? "0" : "")
     << s;
  return ss.str();
}
inline int clampBaseSeconds(int s) {
  return std::clamp(s, 60, 2 * 60 * 60);
}
inline int clampIncSeconds(int s) {
  return std::clamp(s, 0, 30);
}

// Bevel button
void drawBevelButton3D(sf::RenderTarget& t, const sf::FloatRect& r, sf::Color base, bool hovered,
                       bool pressed) {
  // No drop shadow (kept super clean)
  sf::RectangleShape body({r.width, r.height});
  body.setPosition(snapf(r.left), snapf(r.top));
  sf::Color bodyCol = base;
  if (hovered && !pressed) bodyCol = lighten(bodyCol, 8);
  if (pressed) bodyCol = darken(bodyCol, 6);
  body.setFillColor(bodyCol);
  t.draw(body);

  sf::RectangleShape top({r.width, 1.f});
  top.setPosition(snapf(r.left), snapf(r.top));
  top.setFillColor(lighten(bodyCol, 24));
  t.draw(top);
  sf::RectangleShape bot({r.width, 1.f});
  bot.setPosition(snapf(r.left), snapf(r.top + r.height - 1.f));
  bot.setFillColor(darken(bodyCol, 24));
  t.draw(bot);

  sf::RectangleShape inset({r.width - 2.f, r.height - 2.f});
  inset.setPosition(snapf(r.left + 1.f), snapf(r.top + 1.f));
  inset.setFillColor(sf::Color::Transparent);
  inset.setOutlineThickness(1.f);
  inset.setOutlineColor(darken(bodyCol, 18));
  t.draw(inset);
}

void drawAccentInset(sf::RenderTarget& t, const sf::FloatRect& r, sf::Color accent) {
  sf::RectangleShape inset({r.width - 2.f, r.height - 2.f});
  inset.setPosition(snapf(r.left + 1.f), snapf(r.top + 1.f));
  inset.setFillColor(sf::Color::Transparent);
  inset.setOutlineThickness(1.f);
  inset.setOutlineColor(accent);
  t.draw(inset);
}

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

  // Load Game button replaces inline FEN
  const float loadW = PANEL_W * 0.95f;
  const float loadH = 40.f;
  const float loadY = m_startBtn.getPosition().y + m_startBtn.getSize().y + 25.f;
  const float loadX = x0 + (PANEL_W - loadW) * 0.5f;
  m_loadGameBtn.setSize({loadW, loadH});
  m_loadGameBtn.setPosition(snap({loadX, loadY}));
  m_loadGameBtn.setFillColor(colButton);
  m_loadGameBtn.setOutlineThickness(2.f);
  m_loadGameBtn.setOutlineColor(colInputBorder);

  m_loadGameText.setFont(m_font);
  m_loadGameText.setCharacterSize(18);
  m_loadGameText.setFillColor(colText);
  m_loadGameText.setString("Load Game…");
  centerText(m_loadGameText, m_loadGameBtn.getGlobalBounds());

  m_loadSummaryText.setFont(m_font);
  m_loadSummaryText.setCharacterSize(14);
  m_loadSummaryText.setFillColor(colSubtle);
  m_loadSummaryText.setString("Using default starting position");
  m_loadSummaryText.setPosition(snap({loadX, loadY + loadH + 6.f}));

  m_fenInfoText.setFont(m_font);
  m_fenInfoText.setCharacterSize(13);
  m_fenInfoText.setFillColor(colSubtle);
  m_fenInfoText.setString("Provide a FEN or PGN after clicking Load Game.");
  m_fenInfoText.setPosition(snap({loadX, loadY + loadH + 24.f}));

  // Load popup layout
  const float popupW = PANEL_W * 0.8f;
  const float popupH = 360.f;
  const sf::Vector2f popupPos =
      snap({x0 + (PANEL_W - popupW) * 0.5f, y0 + (PANEL_H - popupH) * 0.5f});
  m_fenPopup.setSize({popupW, popupH});
  m_fenPopup.setPosition(popupPos);
  m_fenPopup.setFillColor(colPanel);
  m_fenPopup.setOutlineThickness(2.f);
  m_fenPopup.setOutlineColor(colPanelBorder);

  m_fenLabelText.setFont(m_font);
  m_fenLabelText.setCharacterSize(16);
  m_fenLabelText.setFillColor(colText);
  m_fenLabelText.setString("FEN");
  m_fenLabelText.setPosition(snap({popupPos.x + 24.f, popupPos.y + 24.f}));

  m_fenInputBox.setSize({popupW - 48.f, 44.f});
  m_fenInputBox.setPosition(snap({popupPos.x + 24.f, popupPos.y + 54.f}));
  m_fenInputBox.setFillColor(colInput);
  m_fenInputBox.setOutlineThickness(2.f);
  m_fenInputBox.setOutlineColor(colInputBorder);

  m_fenInputText.setFont(m_font);
  m_fenInputText.setCharacterSize(16);
  m_fenInputText.setFillColor(colText);
  m_fenInputText.setString(m_fenString);
  leftCenterText(m_fenInputText, m_fenInputBox.getGlobalBounds(), 10.f);

  m_fenErrorText.setFont(m_font);
  m_fenErrorText.setCharacterSize(14);
  m_fenErrorText.setFillColor(colInvalid);
  m_fenErrorText.setString("Invalid FEN format");
  m_fenErrorText.setPosition(snap({m_fenInputBox.getPosition().x,
                                   m_fenInputBox.getPosition().y + m_fenInputBox.getSize().y + 6.f}));

  m_pgnLabelText.setFont(m_font);
  m_pgnLabelText.setCharacterSize(16);
  m_pgnLabelText.setFillColor(colText);
  m_pgnLabelText.setString("PGN");
  m_pgnLabelText.setPosition(snap({popupPos.x + 24.f, m_fenErrorText.getPosition().y + 30.f}));

  m_pgnInputBox.setSize({popupW - 48.f, 150.f});
  m_pgnInputBox.setPosition(snap({popupPos.x + 24.f, m_pgnLabelText.getPosition().y + 26.f}));
  m_pgnInputBox.setFillColor(colInput);
  m_pgnInputBox.setOutlineThickness(2.f);
  m_pgnInputBox.setOutlineColor(colInputBorder);

  m_pgnInputText.setFont(m_font);
  m_pgnInputText.setCharacterSize(15);
  m_pgnInputText.setFillColor(colText);
  m_pgnInputText.setString(m_pgnString);
  m_pgnInputText.setPosition(snap({m_pgnInputBox.getPosition().x + 10.f,
                                   m_pgnInputBox.getPosition().y + 8.f}));

  m_pgnErrorText.setFont(m_font);
  m_pgnErrorText.setCharacterSize(14);
  m_pgnErrorText.setFillColor(colInvalid);
  m_pgnErrorText.setString("PGN could not be parsed");
  m_pgnErrorText.setPosition(snap({m_pgnInputBox.getPosition().x,
                                   m_pgnInputBox.getPosition().y + m_pgnInputBox.getSize().y + 6.f}));

  const float btnY = popupPos.y + popupH - 60.f;
  const float btnW = 140.f;
  const float btnH = 38.f;

  m_fenBackBtn.setSize({btnW, btnH});
  m_fenBackBtn.setPosition(snap({popupPos.x + 24.f, btnY}));
  m_fenBackBtn.setFillColor(colButton);
  m_fenBackBtn.setOutlineThickness(0.f);

  m_fenContinueBtn.setSize({btnW, btnH});
  m_fenContinueBtn.setPosition(
      snap({popupPos.x + popupW - btnW - 24.f, btnY}));
  m_fenContinueBtn.setFillColor(colAccent);
  m_fenContinueBtn.setOutlineThickness(0.f);

  m_fenBackText.setFont(m_font);
  m_fenBackText.setCharacterSize(16);
  m_fenBackText.setFillColor(colText);
  m_fenBackText.setString("Back");
  centerText(m_fenBackText, m_fenBackBtn.getGlobalBounds());

  m_fenContinueText.setFont(m_font);
  m_fenContinueText.setCharacterSize(16);
  m_fenContinueText.setFillColor(constant::COL_DARK_TEXT);
  m_fenContinueText.setString("Apply");
  centerText(m_fenContinueText, m_fenContinueBtn.getGlobalBounds());

  // Warning popup layout
  const sf::Vector2f warnSize(420.f, 200.f);
  const sf::Vector2f warnPos =
      snap({x0 + (PANEL_W - warnSize.x) * 0.5f, y0 + (PANEL_H - warnSize.y) * 0.5f});
  m_warningPopup.setSize(warnSize);
  m_warningPopup.setPosition(warnPos);
  m_warningPopup.setFillColor(colPanel);
  m_warningPopup.setOutlineThickness(2.f);
  m_warningPopup.setOutlineColor(colPanelBorder);

  m_warningTitle.setFont(m_font);
  m_warningTitle.setCharacterSize(18);
  m_warningTitle.setFillColor(colText);
  m_warningTitle.setString("Use default values?");
  m_warningTitle.setPosition(snap({warnPos.x + 24.f, warnPos.y + 22.f}));

  m_warningBody.setFont(m_font);
  m_warningBody.setCharacterSize(15);
  m_warningBody.setFillColor(colText);
  m_warningBody.setPosition(snap({warnPos.x + 24.f, warnPos.y + 64.f}));

  m_warningBackBtn.setSize({btnW, btnH});
  m_warningBackBtn.setPosition(snap({warnPos.x + 24.f, warnPos.y + warnSize.y - btnH - 24.f}));
  m_warningBackBtn.setFillColor(colButton);

  m_warningContinueBtn.setSize({btnW, btnH});
  m_warningContinueBtn.setPosition(
      snap({warnPos.x + warnSize.x - btnW - 24.f, warnPos.y + warnSize.y - btnH - 24.f}));
  m_warningContinueBtn.setFillColor(colAccent);

  m_warningBackText.setFont(m_font);
  m_warningBackText.setCharacterSize(16);
  m_warningBackText.setFillColor(colText);
  m_warningBackText.setString("Back");
  centerText(m_warningBackText, m_warningBackBtn.getGlobalBounds());

  m_warningContinueText.setFont(m_font);
  m_warningContinueText.setCharacterSize(16);
  m_warningContinueText.setFillColor(constant::COL_DARK_TEXT);
  m_warningContinueText.setString("Continue");
  centerText(m_warningContinueText, m_warningContinueBtn.getGlobalBounds());

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
  m_loadGameBtn.setOutlineColor(colInputBorder);
  m_loadGameText.setFillColor(colText);
  m_loadSummaryText.setFillColor(colSubtle);
  m_fenInfoText.setFillColor(colSubtle);

  m_fenErrorText.setFillColor(colInvalid);
  m_fenInputBox.setFillColor(colInput);
  m_fenInputBox.setOutlineColor(colInputBorder);
  m_fenInputText.setFillColor(colText);
  m_pgnInputBox.setFillColor(colInput);
  m_pgnInputBox.setOutlineColor(colInputBorder);
  m_pgnInputText.setFillColor(colText);
  m_pgnErrorText.setFillColor(colInvalid);
  m_fenBackBtn.setFillColor(colButton);
  m_fenContinueBtn.setFillColor(colAccent);
  m_fenBackText.setFillColor(colText);
  m_fenContinueText.setFillColor(constant::COL_DARK_TEXT);

  m_warningPopup.setFillColor(colPanel);
  m_warningPopup.setOutlineColor(colPanelBorder);
  m_warningTitle.setFillColor(colText);
  m_warningBody.setFillColor(colText);
  m_warningBackBtn.setFillColor(colButton);
  m_warningBackText.setFillColor(colText);
  m_warningContinueBtn.setFillColor(colAccent);
  m_warningContinueText.setFillColor(constant::COL_DARK_TEXT);

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

  // Start
  if (contains(m_loadGameBtn.getGlobalBounds(), pos)) {
    m_showLoadPopup = true;
    return false;
  }
  if (contains(m_startBtn.getGlobalBounds(), pos)) return true;

  return false;
}

bool StartScreen::isValidFen(const std::string& fen) {
  return basicFenCheck(fen);
}

bool StartScreen::isValidPgn(const std::string& pgn, const std::string& baseFen,
                             std::vector<std::string>* uciMoves) {
  std::string trimmed = trimCopy(pgn);
  if (trimmed.empty()) {
    if (uciMoves) uciMoves->clear();
    return true;
  }

  model::ChessGame game;
  game.setPosition(baseFen);

  std::vector<std::string> collected;
  auto tokens = tokenizePgn(trimmed);
  for (const auto& tok : tokens) {
    if (isMoveNumberToken(tok) || isResultToken(tok)) continue;
    ParsedSan parsed{};
    if (!parseSanToken(tok, parsed)) return false;

    const auto& moves = game.generateLegalMoves();
    bool found = false;
    model::Move chosen;
    if (parsed.castleKing || parsed.castleQueen) {
      for (const auto& mv : moves) {
        if (!mv.isCastle()) continue;
        bool kingSide = mv.castle() == model::CastleSide::KingSide;
        if ((parsed.castleKing && kingSide) || (parsed.castleQueen && !kingSide)) {
          chosen = mv;
          found = true;
          break;
        }
      }
    } else {
      for (const auto& mv : moves) {
        if (mv.to() != parsed.to) continue;
        auto piece = game.getPiece(mv.from());
        if (piece.type != parsed.piece) continue;
        bool captures = mv.isCapture() || mv.isEnPassant();
        if (captures != parsed.capture) continue;
        if (parsed.fileHint && fileChar(mv.from()) != parsed.fileHint) continue;
        if (parsed.rankHint && rankChar(mv.from()) != parsed.rankHint) continue;
        core::PieceType promo = mv.promotion();
        if (promo != parsed.promotion) continue;
        chosen = mv;
        found = true;
        break;
      }
    }

    if (!found) return false;

    game.doMove(chosen.from(), chosen.to(), chosen.promotion());
    collected.push_back(lilia::uci::move_to_uci(chosen));
  }

  if (uciMoves) *uciMoves = std::move(collected);
  return true;
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
  cfg.timeBaseSeconds = m_baseSeconds;
  cfg.timeIncrementSeconds = m_incrementSeconds;
  cfg.timeEnabled = m_timeEnabled;

  bool fenInputActive = false;
  bool pgnInputActive = false;
  const float fenPadX = 10.f;
  const float pgnPadX = 10.f;
  sf::Clock caretClock;
  sf::Clock frameClock;

  auto drawUI = [&](bool fenValid, bool pgnValid, const std::string &trimmedPgn) {
    drawVerticalGradient(m_window, colBGTop, colBGBottom);

    bool palHover = contains(m_paletteButton.getGlobalBounds(), m_mousePos) ||
                    m_showPaletteList || m_paletteListAnim > 0.f;
    m_paletteButton.setFillColor(palHover ? colButtonActive : colButton);
    m_paletteText.setFillColor(colText);
    m_window.draw(m_paletteButton);
    m_window.draw(m_paletteText);
    if (m_paletteListAnim > 0.f) {
      for (std::size_t i = 0; i < m_paletteOptions.size(); ++i) {
        const auto &opt = m_paletteOptions[i];
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
      logoBG.setColor(sf::Color(255, 255, 255, 20));
      auto size = m_window.getSize();
      float scaleX = static_cast<float>(size.x) / static_cast<float>(m_logoTex.getSize().x);
      float scaleY = static_cast<float>(size.y) / static_cast<float>(m_logoTex.getSize().y);
      float scale = std::max(scaleX, scaleY) * 0.6f;
      logoBG.setScale(scale, scale);
      auto bounds = logoBG.getGlobalBounds();
      logoBG.setPosition(snapf(static_cast<float>(size.x) - bounds.width - 30.f), snapf(30.f));
      m_window.draw(logoBG);
    }

    sf::Vector2f panelPos((m_window.getSize().x - PANEL_W) * 0.5f,
                          (m_window.getSize().y - PANEL_H) * 0.5f);
    drawPanelWithShadow(m_window, panelPos);

    if (m_logoTex.getSize().x > 0 && m_logoTex.getSize().y > 0) {
      sf::Sprite logo(m_logoTex);
      logo.setColor(sf::Color(255, 255, 255, 230));
      float scale = 0.35f;
      logo.setScale(scale, scale);
      logo.setPosition(snapf(panelPos.x + 30.f), snapf(panelPos.y + 20.f));
      m_window.draw(logo);
    }

    {
      sf::Text title("LILIA CHESS", m_font, 32);
      title.setFillColor(colText);
      title.setLetterSpacing(1.6f);
      title.setStyle(sf::Text::Bold);
      title.setPosition(snapf(panelPos.x + 120.f), snapf(panelPos.y + 24.f));
      m_window.draw(title);

      sf::Text subtitle("Try different chess bots. Choose sides & engine.", m_font, 18);
      subtitle.setFillColor(colSubtle);
      subtitle.setPosition(snapf(panelPos.x + 24.f), snapf(panelPos.y + 52.f));
      m_window.draw(subtitle);
    }

    m_window.draw(m_whiteLabel);
    m_window.draw(m_blackLabel);

    auto drawSide = [&](sf::RectangleShape &humanBtn, sf::RectangleShape &botBtn,
                        sf::Text &humanTxt, sf::Text &botTxt, bool isBot,
                        bool showList, float anim) {
      auto humanR = humanBtn.getGlobalBounds();
      auto botR = botBtn.getGlobalBounds();
      bool hovH = contains(humanR, m_mousePos);
      bool hovB = contains(botR, m_mousePos) || showList || anim > 0.f;
      bool selH = !isBot;
      bool selB = isBot;
      drawBevelButton3D(m_window, humanR, selH ? colButtonActive : colButton, hovH, selH);
      centerText(humanTxt, humanR);
      m_window.draw(humanTxt);
      if (selH) drawAccentInset(m_window, humanR, colAccent);
      drawBevelButton3D(m_window, botR, selB ? colButtonActive : colButton, hovB, selB);
      centerText(botTxt, botR);
      m_window.draw(botTxt);
      if (selB) drawAccentInset(m_window, botR, colAccent);
    };
    drawSide(m_whitePlayerBtn, m_whiteBotBtn, m_whitePlayerText, m_whiteBotText,
             cfg.whiteIsBot, m_showWhiteBotList, m_whiteBotListAnim);
    drawSide(m_blackPlayerBtn, m_blackBotBtn, m_blackPlayerText, m_blackBotText,
             cfg.blackIsBot, m_showBlackBotList, m_blackBotListAnim);

    auto drawBotList = [&](const std::vector<BotOption> &list, std::size_t selIdx,
                           float anim) {
      if (anim <= 0.f) return;
      for (std::size_t i = 0; i < list.size(); ++i) {
        const auto &opt = list[i];
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
    drawBotList(m_whiteBotOptions, m_whiteBotSelection, m_whiteBotListAnim);
    drawBotList(m_blackBotOptions, m_blackBotSelection, m_blackBotListAnim);

    {
      auto gb = m_timeToggleBtn.getGlobalBounds();
      bool hov = contains(gb, m_mousePos);
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

      m_window.draw(m_timeTitle);

      auto stepBtn = [&](sf::RectangleShape &box, sf::Text &txt, bool hold) {
        auto r = box.getGlobalBounds();
        bool hov = contains(r, m_mousePos);
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
        auto &c = m_presets[i];
        auto r = c.box.getGlobalBounds();
        bool hov = contains(r, m_mousePos);
        bool sel = (m_presetSelection == static_cast<int>(i));
        drawBevelButton3D(m_window, r, sel ? colButtonActive : colButton, hov, sel);
        centerText(c.label, r, -1.f);
        m_window.draw(c.label);
        if (sel) drawAccentInset(m_window, r, colAccent);
      }
    }

    auto startBounds = m_startBtn.getGlobalBounds();
    bool startHover = contains(startBounds, m_mousePos);
    drawBevelButton3D(m_window, startBounds, colAccent, startHover, false);
    centerText(m_startText, startBounds);
    m_window.draw(m_startText);

    bool loadHover = contains(m_loadGameBtn.getGlobalBounds(), m_mousePos);
    sf::Color loadFill = loadHover ? lighten(colButton, 12) : colButton;
    sf::Color loadOutline = colInputBorder;
    if ((!m_fenString.empty() && !fenValid) || (!trimmedPgn.empty() && !pgnValid))
      loadOutline = colInvalid;
    else if ((!m_fenString.empty() && fenValid) || (!trimmedPgn.empty() && pgnValid))
      loadOutline = colValid;
    m_loadGameBtn.setFillColor(loadFill);
    m_loadGameBtn.setOutlineColor(loadOutline);
    m_window.draw(m_loadGameBtn);
    centerText(m_loadGameText, m_loadGameBtn.getGlobalBounds());
    m_window.draw(m_loadGameText);
    m_window.draw(m_loadSummaryText);
    m_window.draw(m_fenInfoText);

    if (m_showLoadPopup || m_showWarningPopup) {
      sf::Vector2u ws = m_window.getSize();
      sf::RectangleShape overlay({static_cast<float>(ws.x), static_cast<float>(ws.y)});
      overlay.setFillColor(sf::Color(0, 0, 0, 120));
      m_window.draw(overlay);
    }

    if (m_showLoadPopup) {
      bool fenHasText = !m_fenString.empty();
      bool trimmedEmpty = trimmedPgn.empty();
      m_fenInputBox.setOutlineColor((!fenHasText || fenValid) ? colInputBorder : colInvalid);
      m_pgnInputBox.setOutlineColor((trimmedEmpty || pgnValid) ? colInputBorder : colInvalid);

      sf::RectangleShape border({m_fenPopup.getSize().x + 4.f, m_fenPopup.getSize().y + 4.f});
      border.setPosition(m_fenPopup.getPosition() - sf::Vector2f{2.f, 2.f});
      border.setFillColor(colPanelBorder);
      m_window.draw(border);
      m_window.draw(m_fenPopup);

      m_window.draw(m_fenLabelText);
      m_window.draw(m_fenInputBox);
      if (fenHasText) {
        m_fenInputText.setString(m_fenString);
        leftCenterText(m_fenInputText, m_fenInputBox.getGlobalBounds(), fenPadX);
        m_window.draw(m_fenInputText);
      } else {
        sf::Text placeholder("STANDARD FEN", m_font, 16);
        placeholder.setFillColor(colSubtle);
        leftCenterText(placeholder, m_fenInputBox.getGlobalBounds(), fenPadX);
        m_window.draw(placeholder);
      }
      if (fenHasText && !fenValid) m_window.draw(m_fenErrorText);

      m_window.draw(m_pgnLabelText);
      m_window.draw(m_pgnInputBox);
      if (trimmedEmpty) {
        sf::Text placeholder("Paste PGN here", m_font, 15);
        placeholder.setFillColor(colSubtle);
        placeholder.setPosition(
            snap({m_pgnInputBox.getPosition().x + pgnPadX, m_pgnInputBox.getPosition().y + 8.f}));
        m_window.draw(placeholder);
      }
      m_pgnInputText.setString(m_pgnString);
      m_pgnInputText.setPosition(
          snap({m_pgnInputBox.getPosition().x + pgnPadX, m_pgnInputBox.getPosition().y + 8.f}));
      m_window.draw(m_pgnInputText);
      if (!trimmedEmpty && !pgnValid) m_window.draw(m_pgnErrorText);

      if (fenInputActive) {
            auto clip = sf::Clipboard::getString().toAnsiString();
            clip.erase(std::remove(clip.begin(), clip.end(), '\n'), clip.end());
            clip.erase(std::remove(clip.begin(), clip.end(), '\r'), clip.end());
            const float avail = m_fenInputBox.getSize().x - 2.f * fenPadX - 2.f;
            std::string out = m_fenString;
            for (char c : clip) {
              sf::Text probe(out + c, m_font, 16);
              if (probe.getLocalBounds().width <= avail)
                out.push_back(c);
              else
                break;
            }
            m_fenString = out;
          } else if (pgnInputActive) {
            auto clip = sf::Clipboard::getString().toAnsiString();
            clip.erase(std::remove(clip.begin(), clip.end(), '\r'), clip.end());
            m_pgnString.append(clip);
          }
 else if (pgnInputActive) {
        float t = std::fmod(caretClock.getElapsedTime().asSeconds(), 1.f);
        if (t < 0.5f) {
          sf::Vector2f pos =
              m_pgnInputText.findCharacterPos(static_cast<std::size_t>(m_pgnString.size()));
          float caretH = m_pgnInputBox.getSize().y - 16.f;
          sf::RectangleShape caret({2.f, caretH});
          caret.setPosition(
              snap({pos.x, m_pgnInputBox.getPosition().y + 8.f}));
          caret.setFillColor(colText);
          m_window.draw(caret);
        }
      }

      drawBevelButton3D(m_window, m_fenBackBtn.getGlobalBounds(), colButton,
                        contains(m_fenBackBtn.getGlobalBounds(), m_mousePos), false);
      centerText(m_fenBackText, m_fenBackBtn.getGlobalBounds());
      m_window.draw(m_fenBackText);
      drawBevelButton3D(m_window, m_fenContinueBtn.getGlobalBounds(), colAccent,
                        contains(m_fenContinueBtn.getGlobalBounds(), m_mousePos), false);
      centerText(m_fenContinueText, m_fenContinueBtn.getGlobalBounds());
      m_window.draw(m_fenContinueText);
    }

    if (m_showWarningPopup) {
      sf::RectangleShape border({m_warningPopup.getSize().x + 4.f, m_warningPopup.getSize().y + 4.f});
      border.setPosition(m_warningPopup.getPosition() - sf::Vector2f{2.f, 2.f});
      border.setFillColor(colPanelBorder);
      m_window.draw(border);
      m_window.draw(m_warningPopup);
      m_window.draw(m_warningTitle);
      m_window.draw(m_warningBody);
      drawBevelButton3D(m_window, m_warningBackBtn.getGlobalBounds(), colButton,
                        contains(m_warningBackBtn.getGlobalBounds(), m_mousePos), false);
      centerText(m_warningBackText, m_warningBackBtn.getGlobalBounds());
      m_window.draw(m_warningBackText);
      drawBevelButton3D(m_window, m_warningContinueBtn.getGlobalBounds(), colAccent,
                        contains(m_warningContinueBtn.getGlobalBounds(), m_mousePos), false);
      centerText(m_warningContinueText, m_warningContinueBtn.getGlobalBounds());
      m_window.draw(m_warningContinueText);
    }

    {
      sf::Text credit("@ 2025 Julian Meyer", m_font, 13);
      credit.setFillColor(colSubtle);
      auto cb = credit.getLocalBounds();
      sf::Vector2u ws = m_window.getSize();
      credit.setPosition(snapf(static_cast<float>(ws.x) - cb.width - 18.f),
                         snapf(static_cast<float>(ws.y) - cb.height - 22.f));
      m_window.draw(credit);
    }
  };

  auto updateHoverStates = [&](sf::Vector2f mouse) {
    auto updateHover = [&](bool &show, bool &forceHide, const sf::FloatRect &btn,
                           const auto &options) {
      bool overBtn = contains(btn, mouse);
      bool overList = false;
      for (const auto &opt : options) {
        if (contains(opt.box.getGlobalBounds(), mouse)) {
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
  };

  auto animateLists = [&](float dt) {
    auto animate = [&](bool show, float &anim) {
      const float speed = 10.f;
      if (show)
        anim = std::min(1.f, anim + speed * dt);
      else
        anim = std::max(0.f, anim - speed * dt);
    };
    animate(m_showPaletteList, m_paletteListAnim);
    animate(m_showWhiteBotList, m_whiteBotListAnim);
    animate(m_showBlackBotList, m_blackBotListAnim);
  };

  while (m_window.isOpen()) {
    float dt = frameClock.restart().asSeconds();
    bool requestStart = false;
    bool warningAccepted = false;

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
        m_mousePos = {static_cast<float>(e.mouseMove.x), static_cast<float>(e.mouseMove.y)};
        updateHoverStates(m_mousePos);
      }

      if (m_showWarningPopup) {
        if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
          sf::Vector2f mp(static_cast<float>(e.mouseButton.x), static_cast<float>(e.mouseButton.y));
          if (contains(m_warningBackBtn.getGlobalBounds(), mp)) {
            m_showWarningPopup = false;
          } else if (contains(m_warningContinueBtn.getGlobalBounds(), mp)) {
            m_showWarningPopup = false;
            warningAccepted = true;
          }
        } else if (e.type == sf::Event::KeyPressed) {
          if (e.key.code == sf::Keyboard::Escape) {
            m_showWarningPopup = false;
          } else if (e.key.code == sf::Keyboard::Enter || e.key.code == sf::Keyboard::Return) {
            m_showWarningPopup = false;
            warningAccepted = true;
          }
        }
        continue;
      }

      if (m_showLoadPopup) {
        if (e.type == sf::Event::KeyPressed) {
          if (e.key.code == sf::Keyboard::Escape) {
            m_showLoadPopup = false;
            fenInputActive = false;
            pgnInputActive = false;
          } else if ((e.key.control || e.key.system) && e.key.code == sf::Keyboard::V) {
            if (fenInputActive) {
              auto clip = sf::Clipboard::getString().toAnsiString();
              clip.erase(std::remove(clip.begin(), clip.end(), '\r'), clip.end());
              clip.erase(std::remove(clip.begin(), clip.end(), '\n'), clip.end());
              const float avail = m_fenInputBox.getSize().x - 2.f * fenPadX - 2.f;
              std::string out = m_fenString;
              for (char c : clip) {
                sf::Text probe(out + c, m_font, 16);
                if (probe.getLocalBounds().width <= avail)
                  out.push_back(c);
                else
                  break;
              }
              m_fenString = out;
            } else if (pgnInputActive) {
              auto clip = sf::Clipboard::getString().toAnsiString();
              clip.erase(std::remove(clip.begin(), clip.end(), '\r'), clip.end());
              m_pgnString.append(clip);
            }
          }
        } else if (e.type == sf::Event::MouseButtonPressed &&
                   e.mouseButton.button == sf::Mouse::Left) {
          sf::Vector2f mp(static_cast<float>(e.mouseButton.x), static_cast<float>(e.mouseButton.y));
          if (contains(m_fenInputBox.getGlobalBounds(), mp)) {
            fenInputActive = true;
            pgnInputActive = false;
            caretClock.restart();
          } else if (contains(m_pgnInputBox.getGlobalBounds(), mp)) {
            pgnInputActive = true;
            fenInputActive = false;
            caretClock.restart();
          } else if (contains(m_fenBackBtn.getGlobalBounds(), mp)) {
            m_showLoadPopup = false;
            fenInputActive = pgnInputActive = false;
          } else if (contains(m_fenContinueBtn.getGlobalBounds(), mp)) {
            m_showLoadPopup = false;
            fenInputActive = pgnInputActive = false;
          } else if (!contains(m_fenPopup.getGlobalBounds(), mp)) {
            m_showLoadPopup = false;
            fenInputActive = pgnInputActive = false;
          } else {
            fenInputActive = false;
            pgnInputActive = false;
          }
        } else if (e.type == sf::Event::TextEntered) {
          if (fenInputActive) {
            if (e.text.unicode == 8) {
              if (!m_fenString.empty()) m_fenString.pop_back();
            } else if (e.text.unicode >= 32 && e.text.unicode < 127) {
              const float avail = m_fenInputBox.getSize().x - 2.f * fenPadX - 2.f;
              std::string tmp = m_fenString;
              tmp.push_back(static_cast<char>(e.text.unicode));
              sf::Text probe(tmp, m_font, 16);
              if (probe.getLocalBounds().width <= avail) m_fenString.push_back(static_cast<char>(e.text.unicode));
            }
          } else if (pgnInputActive) {
            if (e.text.unicode == 8) {
              if (!m_pgnString.empty()) m_pgnString.pop_back();
            } else if (e.text.unicode == '\n' || e.text.unicode == '\n') {
              m_pgnString.push_back('\n');
            } else if (e.text.unicode >= 32 && e.text.unicode < 127) {
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
        } else if (e.key.code == sf::Keyboard::Enter || e.key.code == sf::Keyboard::Return) {
          requestStart = true;
        }
      }

      if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2f mp(static_cast<float>(e.mouseButton.x), static_cast<float>(e.mouseButton.y));
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
          if (handleMouse(mp, cfg)) requestStart = true;
        }
      }

      if (e.type == sf::Event::MouseButtonReleased && e.mouseButton.button == sf::Mouse::Left) {
        m_holdBaseMinus.active = m_holdBasePlus.active = m_holdIncMinus.active =
            m_holdIncPlus.active = false;
      }
    }

    if (!m_window.isOpen()) break;

    if (m_timeEnabled) {
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

    animateLists(dt);

    std::string trimmedPgn = trimCopy(m_pgnString);
    bool fenValid = m_fenString.empty() || isValidFen(m_fenString);
    std::string baseFen = (fenValid && !m_fenString.empty()) ? m_fenString : core::START_FEN;
    std::vector<std::string> parsedMoves;
    bool pgnValid = isValidPgn(trimmedPgn, baseFen, &parsedMoves);
    if (pgnValid)
      m_cachedPgnMoves = parsedMoves;
    else
      m_cachedPgnMoves.clear();

    std::string summary;
    if (!m_fenString.empty() && !fenValid)
      summary = "FEN needs fixing";
    else if (!trimmedPgn.empty() && !pgnValid)
      summary = "PGN needs fixing";
    else if (!m_fenString.empty() && fenValid && !trimmedPgn.empty() && pgnValid)
      summary = "Custom FEN & PGN ready";
    else if (!m_fenString.empty() && fenValid)
      summary = "Custom FEN ready";
    else if (!trimmedPgn.empty() && pgnValid)
      summary = "PGN loaded (" + std::to_string(parsedMoves.size()) + " moves)";
    else
      summary = "Using default starting position";
    m_loadSummaryText.setString(summary);

    if (warningAccepted) {
      cfg.timeBaseSeconds = m_baseSeconds;
      cfg.timeIncrementSeconds = m_incrementSeconds;
      cfg.timeEnabled = m_timeEnabled;
      cfg.fen = (fenValid && !m_fenString.empty()) ? m_fenString : core::START_FEN;
      if (trimmedPgn.empty() || !pgnValid) {
        cfg.pgn.clear();
        cfg.pgnMoves.clear();
      } else {
        cfg.pgn = trimmedPgn;
        cfg.pgnMoves = parsedMoves;
      }
      if (!fenValid) cfg.fen = core::START_FEN;
      if (!pgnValid) {
        cfg.pgn.clear();
        cfg.pgnMoves.clear();
      }
      return cfg;
    }

    if (requestStart) {
      bool invalidFen = !m_fenString.empty() && !fenValid;
      bool invalidPgn = !trimmedPgn.empty() && !pgnValid;
      if (invalidFen || invalidPgn) {
        m_showWarningPopup = true;
        m_warningInvalidFen = invalidFen;
        m_warningInvalidPgn = invalidPgn;
        std::string warn;
        if (invalidFen) warn += "FEN is invalid. Default start position will be used.
";
        if (invalidPgn) warn += "PGN is invalid and will be ignored.";
        if (warn.empty()) warn = "Invalid input.";
        m_warningBody.setString(warn);
      } else {
        cfg.timeBaseSeconds = m_baseSeconds;
        cfg.timeIncrementSeconds = m_incrementSeconds;
        cfg.timeEnabled = m_timeEnabled;
        cfg.fen = (!m_fenString.empty()) ? m_fenString : core::START_FEN;
        cfg.pgn = trimmedPgn;
        cfg.pgnMoves = parsedMoves;
        return cfg;
      }
    }

    m_window.clear();
    drawUI(fenValid, pgnValid, trimmedPgn);
    m_window.display();
  }

  std::string trimmedPgn = trimCopy(m_pgnString);
  bool fenValid = m_fenString.empty() || isValidFen(m_fenString);
  std::string baseFen = (fenValid && !m_fenString.empty()) ? m_fenString : core::START_FEN;
  std::vector<std::string> parsedMoves;
  bool pgnValid = isValidPgn(trimmedPgn, baseFen, &parsedMoves);
  cfg.timeBaseSeconds = m_baseSeconds;
  cfg.timeIncrementSeconds = m_incrementSeconds;
  cfg.timeEnabled = m_timeEnabled;
  cfg.fen = (fenValid && !m_fenString.empty()) ? m_fenString : core::START_FEN;
  if (pgnValid) {
    cfg.pgn = trimmedPgn;
    cfg.pgnMoves = parsedMoves;
  }
  return cfg;
}


}  // namespace lilia::view
