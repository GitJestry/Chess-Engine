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
const sf::Color kAccent(100, 190, 255);

// layout
constexpr float kScale = 0.80f;  // 20% smaller
constexpr float kPadX = 10.f;
constexpr float kPadY = 6.f;
constexpr float kIconRadius = 6.f;
constexpr float kIconOffsetX = kIconRadius + 8.f;  // small left margin
constexpr float kActiveStripW = 3.f;

inline float snapf(float v) {
  return std::round(v);
}

inline sf::Color lighten(sf::Color c, int d) {
  auto clip = [](int x) { return std::clamp(x, 0, 255); };
  return sf::Color(clip(c.r + d), clip(c.g + d), clip(c.b + d), c.a);
}
inline sf::Color darken(sf::Color c, int d) {
  return lighten(c, -d);
}
inline sf::Color lerp(sf::Color a, sf::Color b, float t) {
  auto L = [&](int A, int B) { return static_cast<sf::Uint8>(std::lround(A + (B - A) * t)); };
  return sf::Color(L(a.r, b.r), L(a.g, b.g), L(a.b, b.b), L(a.a, b.a));
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

  // Overlay: used to dim inactive clock / gently tint active
  m_overlay.setSize({baseW, baseH});
  m_overlay.setFillColor(sf::Color(0, 0, 0, 100));  // default: dim

  // Small "analog" indicator (only when active)
  m_icon_circle.setRadius(kIconRadius);
  m_icon_circle.setOrigin(kIconRadius, kIconRadius);
  m_icon_circle.setFillColor(sf::Color::Transparent);
  m_icon_circle.setOutlineThickness(3.f);   // stays crisp
  m_icon_circle.setOutlineColor(kOutline);  // setPlayerColor() will adjust

  m_icon_hand.setSize({kIconRadius - 2.f, 1.f});
  m_icon_hand.setFillColor(kOutline);  // setPlayerColor() will adjust
  m_icon_hand.setOutlineThickness(1.f);
  m_icon_hand.setOutlineColor(kOutline);
  m_icon_hand.setOrigin(0.f, 0.5f);
  m_icon_hand.setRotation(-90.f);  // up

  m_font.loadFromFile(constant::STR_FILE_PATH_FONT);
  m_font.setSmooth(false);

  m_text.setFont(m_font);
  m_text.setCharacterSize(18);
  m_text.setFillColor(kLightText);  // setPlayerColor() will adjust
  m_text.setStyle(sf::Text::Style::Bold);
}

void Clock::setPlayerColor(core::Color color) {
  if (color == core::Color::White) {
    m_box.setFillColor(kLightBG);
    m_text.setFillColor(kDarkText);
    // Darker accent so it pops on light bg
    sf::Color iconCol = lerp(kAccent, kDarkText, 0.45f);
    m_icon_circle.setOutlineColor(iconCol);
    m_icon_hand.setFillColor(iconCol);
  } else {
    m_box.setFillColor(kDarkBG);
    m_text.setFillColor(kLightText);
    // Lighter accent so it pops on dark bg
    sf::Color iconCol = lerp(kAccent, kLightText, 0.25f);
    m_icon_circle.setOutlineColor(iconCol);
    m_icon_hand.setFillColor(iconCol);
  }
}

void Clock::setPosition(const sf::Vector2f& pos) {
  // position the box & overlay
  m_box.setPosition({snapf(pos.x), snapf(pos.y)});
  m_overlay.setPosition(m_box.getPosition());

  // right-align the time inside the box (like chess.com)
  const auto tb = m_text.getLocalBounds();  // tb.top usually negative in SFML
  const auto bs = m_box.getSize();

  const float tx = m_box.getPosition().x + bs.x - kPadX - tb.width;
  const float ty = m_box.getPosition().y + (bs.y - tb.height) * 0.5f - tb.top;
  m_text.setPosition({snapf(tx), snapf(ty)});

  // place the small indicator in the vertical center, a bit from left edge
  const float iconX = m_box.getPosition().x + kIconOffsetX;
  const float iconY = m_box.getPosition().y + bs.y * 0.5f;
  m_icon_circle.setPosition({snapf(iconX), snapf(iconY)});
  m_icon_hand.setPosition({snapf(iconX), snapf(iconY)});
}

void Clock::setTime(float seconds) {
  m_text.setString(formatTime(seconds));

  // ensure the text fits: grow width if needed (height stays the same)
  const auto tb = m_text.getLocalBounds();
  auto size = m_box.getSize();
  const float minW = WIDTH * kScale;  // baseline
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

  // reposition text after potential resize
  setPosition(m_box.getPosition());
}

void Clock::setActive(bool active) {
  m_active = active;

  // Determine base theme by text color (avoids adding members)
  const bool isLightTheme = (m_text.getFillColor() == kDarkText);
  const sf::Color baseFill = isLightTheme ? kLightBG : kDarkBG;

  if (active) {
    // Manipulate fill for clarity on both themes:
    const sf::Color tweakedFill = isLightTheme ? darken(baseFill, 18) : lighten(baseFill, 16);
    m_box.setFillColor(tweakedFill);

    // Accent outline
    m_box.setOutlineThickness(2.f);
    m_box.setOutlineColor(lerp(kOutline, kAccent, 0.65f));

    // Gentle accent tint overlay
    sf::Color tint = kAccent;
    tint.a = 28;  // subtle
    m_overlay.setFillColor(tint);
  } else {
    // restore neutral appearance
    m_box.setFillColor(baseFill);
    m_box.setOutlineThickness(1.f);
    m_box.setOutlineColor(kOutline);
    m_overlay.setFillColor(sf::Color(0, 0, 0, 100));  // dim
    // reset hand to "12 o'clock" when inactive
    m_icon_hand.setRotation(-90.f);
  }
}

void Clock::render(sf::RenderWindow& window) {
  // base box + overlay
  window.draw(m_box);
  window.draw(m_overlay);

  // left accent strip + analog indicator when active
  if (m_active) {
    // thin accent strip
    sf::RectangleShape strip({kActiveStripW, m_box.getSize().y});
    strip.setPosition(m_box.getPosition());
    strip.setFillColor(kAccent);
    window.draw(strip);

    // --- simple ticking animation: 90° per second ---
    static sf::Clock animClock;  // shared ticking base (keeps code header-free)
    const int step = static_cast<int>(animClock.getElapsedTime().asSeconds()) % 4;
    const float angle = -90.f + 90.f * static_cast<float>(step);  // up, right, down, left
    m_icon_hand.setRotation(angle);

    // draw indicator
    window.draw(m_icon_circle);
    window.draw(m_icon_hand);
  }

  // time text
  window.draw(m_text);
}

}  // namespace lilia::view
