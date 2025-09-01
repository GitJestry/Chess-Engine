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
constexpr float kIconRadius = 6.f;
constexpr float kIconOffsetX = kIconRadius + 2.f;  // keep a small left margin

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
  m_box.setOutlineThickness(2.f);
  m_box.setOutlineColor(kOutline);

  m_overlay.setSize({baseW, baseH});
  m_overlay.setFillColor(sf::Color(0, 0, 0, 100));

  m_icon_circle.setRadius(kIconRadius);
  m_icon_circle.setOrigin(kIconRadius, kIconRadius);
  m_icon_circle.setFillColor(sf::Color::Transparent);
  m_icon_circle.setOutlineThickness(1.f);
  m_icon_circle.setOutlineColor(kOutline);
  m_icon_hand.setSize({kIconRadius - 2.f, 1.f});
  m_icon_hand.setFillColor(kOutline);
  m_icon_hand.setOrigin(0.f, 0.5f);
  m_icon_hand.setRotation(-90.f);

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
  m_overlay.setPosition(m_box.getPosition());

  // right-align the time inside the box (like chess.com)
  const auto tb = m_text.getLocalBounds();  // tb.top is usually negative in SFML
  const auto bs = m_box.getSize();

  const float tx = m_box.getPosition().x + bs.x - kPadX - tb.width;
  const float ty = m_box.getPosition().y + (bs.y - tb.height) * 0.5f - tb.top;

  m_text.setPosition({snapf(tx), snapf(ty)});

  const float iconX = m_box.getPosition().x + kIconOffsetX;
  const float iconY = m_box.getPosition().y + bs.y * 0.5f;
  m_icon_circle.setPosition({snapf(iconX), snapf(iconY)});
  m_icon_hand.setPosition({snapf(iconX), snapf(iconY)});
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
    m_overlay.setSize(size);
  } else if (size.x < minW) {
    size.x = minW;
    m_box.setSize(size);
    m_overlay.setSize(size);
  }

  // reposition text for right-alignment with the (possibly) new width
  setPosition(m_box.getPosition());
}

void Clock::setActive(bool active) {
  m_active = active;
  if (active) {
    m_overlay.setFillColor(sf::Color(255, 255, 255, 40));
  } else {
    m_overlay.setFillColor(sf::Color(0, 0, 0, 100));
  }
}

void Clock::render(sf::RenderWindow& window) {
  window.draw(m_box);
  window.draw(m_overlay);
  if (m_active) {
    window.draw(m_icon_circle);
    window.draw(m_icon_hand);
  }
  window.draw(m_text);
}

}  // namespace lilia::view
