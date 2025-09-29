#pragma once

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>

namespace lilia::view {

inline float snapf(float v) {
  return std::round(v);
}

inline sf::Vector2f snap(sf::Vector2f v) {
  return {snapf(v.x), snapf(v.y)};
}

inline sf::Color lighten(sf::Color c, int delta) {
  auto clip = [](int value) { return std::clamp(value, 0, 255); };
  return sf::Color(clip(c.r + delta), clip(c.g + delta), clip(c.b + delta), c.a);
}

inline sf::Color darken(sf::Color c, int delta) {
  return lighten(c, -delta);
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

inline void drawBevelButton3D(sf::RenderTarget& target, const sf::FloatRect& bounds, sf::Color base,
                              bool hovered, bool pressed) {
  sf::RectangleShape body({bounds.width, bounds.height});
  body.setPosition(snapf(bounds.left), snapf(bounds.top));
  sf::Color bodyCol = base;
  if (hovered && !pressed) bodyCol = lighten(bodyCol, 8);
  if (pressed) bodyCol = darken(bodyCol, 6);
  body.setFillColor(bodyCol);
  target.draw(body);

  sf::RectangleShape top({bounds.width, 1.f});
  top.setPosition(snapf(bounds.left), snapf(bounds.top));
  top.setFillColor(lighten(bodyCol, 24));
  target.draw(top);

  sf::RectangleShape bot({bounds.width, 1.f});
  bot.setPosition(snapf(bounds.left), snapf(bounds.top + bounds.height - 1.f));
  bot.setFillColor(darken(bodyCol, 24));
  target.draw(bot);

  sf::RectangleShape inset({bounds.width - 2.f, bounds.height - 2.f});
  inset.setPosition(snapf(bounds.left + 1.f), snapf(bounds.top + 1.f));
  inset.setFillColor(sf::Color::Transparent);
  inset.setOutlineThickness(1.f);
  inset.setOutlineColor(darken(bodyCol, 18));
  target.draw(inset);
}

inline void drawAccentInset(sf::RenderTarget& target, const sf::FloatRect& bounds, sf::Color accent) {
  sf::RectangleShape inset({bounds.width - 2.f, bounds.height - 2.f});
  inset.setPosition(snapf(bounds.left + 1.f), snapf(bounds.top + 1.f));
  inset.setFillColor(sf::Color::Transparent);
  inset.setOutlineThickness(1.f);
  inset.setOutlineColor(accent);
  target.draw(inset);
}

}  // namespace lilia::view

