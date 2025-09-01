#include "lilia/view/clock.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

#include "lilia/view/render_constants.hpp"

namespace lilia::view {

namespace {
// Theme colors (in sync with the app)
const sf::Color kOutline(120, 140, 170, 180);
const sf::Color kLightBG(210, 215, 230);    // white side clock bg
const sf::Color kDarkBG(33, 38, 50);        // black side clock bg
const sf::Color kDarkText(26, 22, 30);      // text on light bg
const sf::Color kLightText(210, 224, 255);  // text on dark bg

// layout
constexpr float kScale = 0.80f;  // 20% smaller
constexpr float kPadX = 10.f;
constexpr float kPadY = 6.f;

inline float snapf(float v) {
  return std::round(v);
}

static std::string formatTime(float seconds) {
  int s = static_cast<int>(seconds + 0.5f);
  int h = s / 3600;
  int m = (s % 3600) / 60;
  int sec = s % 60;

  std::ostringstream oss;
  oss << std::setw(2) << std::setfill('0');
  if (h > 0) {
    oss << h << ':' << std::setw(2) << m << ':' << std::setw(2) << sec;
  } else {
    oss << m << ':' << std::setw(2) << sec;
  }
  return oss.str();
}

}  // namespace

Clock::Clock() {
  // 20% smaller box
  const float baseW = WIDTH * kScale;
  const float baseH = HEIGHT * kScale;

  m_box.setSize({baseW, baseH});
  m_box.setOutlineThickness(1.f);
  m_box.setOutlineColor(kOutline);

  m_font.loadFromFile(constant::STR_FILE_PATH_FONT);
  m_font.setSmooth(false);

  m_text.setFont(m_font);
  m_text.setCharacterSize(18);
  m_text.setFillColor(kLightText);  // default; setPlayerColor() will adjust
}

void Clock::setPlayerColor(core::Color color) {
  if (color == core::Color::White) {
    m_box.setFillColor(kLightBG);
    m_text.setFillColor(kDarkText);
  } else {
    m_box.setFillColor(kDarkBG);
    m_text.setFillColor(kLightText);
  }
}

void Clock::setPosition(const sf::Vector2f& pos) {
  // position the box
  m_box.setPosition({snapf(pos.x), snapf(pos.y)});

  // right-align the time inside the box (like chess.com)
  const auto tb = m_text.getLocalBounds();  // tb.top is usually negative in SFML
  const auto bs = m_box.getSize();

  const float tx = m_box.getPosition().x + bs.x - kPadX - tb.width;
  const float ty = m_box.getPosition().y + (bs.y - tb.height) * 0.5f - tb.top;

  m_text.setPosition({snapf(tx), snapf(ty)});
}

void Clock::setTime(float seconds) {
  // update string
  m_text.setString(formatTime(seconds));

  // ensure the text fits: grow width if needed (height stays the same)
  const auto tb = m_text.getLocalBounds();
  auto size = m_box.getSize();
  const float minW = WIDTH * kScale;  // baseline width (20% smaller than original)
  const float needW = tb.width + 2.f * kPadX;

  if (needW > size.x) {
    size.x = needW;
    m_box.setSize(size);
  } else if (size.x < minW) {
    size.x = minW;
    m_box.setSize(size);
  }

  // reposition text for right-alignment with the (possibly) new width
  setPosition(m_box.getPosition());
}

void Clock::render(sf::RenderWindow& window) {
  window.draw(m_box);
  window.draw(m_text);
}

}  // namespace lilia::view
