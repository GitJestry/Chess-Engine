#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

#include "lilia/bot/bot_info.hpp"

namespace lilia::view::start_screen::ui {

float snapf(float v);
sf::Vector2f snap(sf::Vector2f v);
void centerText(sf::Text &t, const sf::FloatRect &box, float dy = 0.f);
void leftCenterText(sf::Text &t, const sf::FloatRect &box, float padX, float dy = 0.f);
void drawVerticalGradient(sf::RenderWindow &window, sf::Color top, sf::Color bottom);
sf::Color lighten(sf::Color c, int delta);
sf::Color darken(sf::Color c, int delta);
void drawBevelButton3D(sf::RenderTarget &target, const sf::FloatRect &rect, sf::Color base,
                       bool hovered, bool pressed);
void drawAccentInset(sf::RenderTarget &target, const sf::FloatRect &rect, sf::Color accent);
std::vector<BotType> availableBots();
std::string botDisplayName(BotType type);
bool basicFenCheck(const std::string &fen);
bool basicPgnCheck(const std::string &pgn);
std::string formatHMS(int totalSeconds);
int clampBaseSeconds(int seconds);
int clampIncSeconds(int seconds);

template <typename T>
bool contains(const sf::Rect<T> &rect, sf::Vector2f point) {
  return rect.contains(point);
}

}  // namespace lilia::view::start_screen::ui
