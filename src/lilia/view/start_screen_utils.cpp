#include "lilia/view/start_screen_utils.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <string_view>

namespace lilia::view::start_screen::ui {

float snapf(float v) {
  return std::round(v);
}

sf::Vector2f snap(sf::Vector2f v) {
  return {snapf(v.x), snapf(v.y)};
}

void centerText(sf::Text &t, const sf::FloatRect &box, float dy) {
  auto b = t.getLocalBounds();
  t.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
  t.setPosition(snapf(box.left + box.width / 2.f), snapf(box.top + box.height / 2.f + dy));
}

void leftCenterText(sf::Text &t, const sf::FloatRect &box, float padX, float dy) {
  auto b = t.getLocalBounds();
  t.setOrigin(b.left, b.top + b.height / 2.f);
  t.setPosition(snapf(box.left + padX), snapf(box.top + box.height / 2.f + dy));
}

void drawVerticalGradient(sf::RenderWindow &window, sf::Color top, sf::Color bottom) {
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

sf::Color lighten(sf::Color c, int delta) {
  auto clip = [](int x) { return std::clamp(x, 0, 255); };
  return sf::Color(clip(c.r + delta), clip(c.g + delta), clip(c.b + delta), c.a);
}

sf::Color darken(sf::Color c, int delta) {
  return lighten(c, -delta);
}

void drawBevelButton3D(sf::RenderTarget &target, const sf::FloatRect &rect, sf::Color base,
                       bool hovered, bool pressed) {
  sf::RectangleShape body({rect.width, rect.height});
  body.setPosition(snapf(rect.left), snapf(rect.top));
  sf::Color bodyCol = base;
  if (hovered && !pressed) bodyCol = lighten(bodyCol, 8);
  if (pressed) bodyCol = darken(bodyCol, 6);
  body.setFillColor(bodyCol);
  target.draw(body);

  sf::RectangleShape top({rect.width, 1.f});
  top.setPosition(snapf(rect.left), snapf(rect.top));
  top.setFillColor(lighten(bodyCol, 24));
  target.draw(top);

  sf::RectangleShape bot({rect.width, 1.f});
  bot.setPosition(snapf(rect.left), snapf(rect.top + rect.height - 1.f));
  bot.setFillColor(darken(bodyCol, 24));
  target.draw(bot);

  sf::RectangleShape inset({rect.width - 2.f, rect.height - 2.f});
  inset.setPosition(snapf(rect.left + 1.f), snapf(rect.top + 1.f));
  inset.setFillColor(sf::Color::Transparent);
  inset.setOutlineThickness(1.f);
  inset.setOutlineColor(darken(bodyCol, 18));
  target.draw(inset);
}

void drawAccentInset(sf::RenderTarget &target, const sf::FloatRect &rect, sf::Color accent) {
  sf::RectangleShape inset({rect.width - 2.f, rect.height - 2.f});
  inset.setPosition(snapf(rect.left + 1.f), snapf(rect.top + 1.f));
  inset.setFillColor(sf::Color::Transparent);
  inset.setOutlineThickness(1.f);
  inset.setOutlineColor(accent);
  target.draw(inset);
}

std::vector<BotType> availableBots() {
  return {BotType::Lilia};
}

std::string botDisplayName(BotType type) {
  return getBotConfig(type).info.name;
}

bool basicFenCheck(const std::string &fen) {
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
  if (!(fields[2] == "-" || fields[2].find_first_not_of("KQkq") == std::string::npos))
    return false;
  if (!(fields[3] == "-")) {
    if (fields[3].size() != 2) return false;
    if (fields[3][0] < 'a' || fields[3][0] > 'h') return false;
    if (!(fields[3][1] == '3' || fields[3][1] == '6')) return false;
  }
  auto isNonNegInt = [](const std::string &s) {
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

namespace {

std::string trim(std::string_view view) {
  auto left = view.find_first_not_of(" \t\r\n");
  if (left == std::string_view::npos) return "";
  auto right = view.find_last_not_of(" \t\r\n");
  return std::string(view.substr(left, right - left + 1));
}

bool isResultToken(const std::string &token) {
  return token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*";
}

bool isMoveNumberToken(const std::string &token) {
  if (token.empty()) return false;
  if (token.back() != '.') return false;
  for (char c : token) {
    if (c != '.' && !std::isdigit(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

bool looksLikeSan(const std::string &token) {
  if (token.empty()) return false;
  for (char c : token) {
    if (std::isalnum(static_cast<unsigned char>(c))) continue;
    switch (c) {
      case '+':
      case '#':
      case '=':
      case '-':
      case 'O':
      case '!':
      case '?':
      case '/':
        break;
      default:
        return false;
    }
  }
  return true;
}

}  // namespace

bool basicPgnCheck(const std::string &pgn) {
  std::istringstream stream(pgn);
  std::string line;
  bool sawMoves = false;
  bool hasContent = false;

  while (std::getline(stream, line)) {
    std::string trimmed = trim(line);
    if (trimmed.empty()) continue;
    hasContent = true;

    if (trimmed.front() == '[') {
      if (trimmed.back() != ']') return false;
      continue;
    }

    std::istringstream moves(trimmed);
    std::string token;
    while (moves >> token) {
      if (isResultToken(token) || isMoveNumberToken(token)) continue;
      if (!looksLikeSan(token)) return false;
      sawMoves = true;
    }
  }

  return hasContent && sawMoves;
}

std::string formatHMS(int totalSeconds) {
  totalSeconds = std::max(0, totalSeconds);
  int h = totalSeconds / 3600;
  int m = (totalSeconds % 3600) / 60;
  int s = totalSeconds % 60;
  std::ostringstream ss;
  ss << (h < 10 ? "0" : "") << h << ":" << (m < 10 ? "0" : "") << m << ":"
     << (s < 10 ? "0" : "") << s;
  return ss.str();
}

int clampBaseSeconds(int seconds) {
  return std::clamp(seconds, 60, 2 * 60 * 60);
}

int clampIncSeconds(int seconds) {
  return std::clamp(seconds, 0, 30);
}

}  // namespace lilia::view::start_screen::ui
