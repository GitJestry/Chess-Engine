#include "lilia/view/clock.hpp"

#include <iomanip>
#include <sstream>
#include <string>

namespace lilia::view {

Clock::Clock() {
  m_box.setSize({WIDTH, HEIGHT});
  m_box.setOutlineThickness(2.f);
  m_box.setOutlineColor(sf::Color(80, 80, 80));
  m_font.loadFromFile(constant::STR_FILE_PATH_FONT);
  m_text.setFont(m_font);
  m_text.setCharacterSize(18);
  m_text.setFillColor(sf::Color::Black);
}

void Clock::setPlayerColor(core::Color color) {
  if (color == core::Color::White) {
    m_box.setFillColor(sf::Color(230, 230, 230));
    m_text.setFillColor(sf::Color::Black);
  } else {
    m_box.setFillColor(sf::Color(40, 40, 40));
    m_text.setFillColor(sf::Color::White);
  }
}

void Clock::setPosition(const sf::Vector2f& pos) {
  m_box.setPosition(pos);
  m_text.setPosition(pos.x + 10.f, pos.y + 8.f);
}

static std::string formatTime(float seconds) {
  int s = static_cast<int>(seconds + 0.5f);
  int h = s / 3600;
  int m = (s % 3600) / 60;
  int sec = s % 60;
  std::ostringstream oss;
  if (h > 0) {
    oss << std::setw(2) << std::setfill('0') << h << ':' << std::setw(2)
        << m << ':' << std::setw(2) << sec;
  } else {
    oss << std::setw(2) << std::setfill('0') << m << ':' << std::setw(2)
        << sec;
  }
  return oss.str();
}

void Clock::setTime(float seconds) {
  m_text.setString(formatTime(seconds));
}

void Clock::render(sf::RenderWindow& window) {
  window.draw(m_box);
  window.draw(m_text);
}

}  // namespace lilia::view

