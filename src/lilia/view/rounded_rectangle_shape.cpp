#include "lilia/view/rounded_rectangle_shape.hpp"

#include <cmath>

namespace lilia::view {

RoundedRectangleShape::RoundedRectangleShape(const sf::Vector2f& size,
                                             float radius,
                                             std::size_t cornerPointCount)
    : m_size(size), m_radius(radius), m_cornerPointCount(cornerPointCount) {
  update();
}

void RoundedRectangleShape::setSize(const sf::Vector2f& size) {
  m_size = size;
  update();
}

const sf::Vector2f& RoundedRectangleShape::getSize() const { return m_size; }

void RoundedRectangleShape::setCornersRadius(float radius) {
  m_radius = radius;
  update();
}

float RoundedRectangleShape::getCornersRadius() const { return m_radius; }

void RoundedRectangleShape::setCornerPointCount(std::size_t count) {
  m_cornerPointCount = count;
  update();
}

std::size_t RoundedRectangleShape::getPointCount() const {
  return m_cornerPointCount * 4;
}

sf::Vector2f RoundedRectangleShape::getPoint(std::size_t index) const {
  if (m_radius == 0) {
    switch (index) {
      case 0:
        return {0.f, 0.f};
      case 1:
        return {m_size.x, 0.f};
      case 2:
        return {m_size.x, m_size.y};
      default:
        return {0.f, m_size.y};
    }
  }

  std::size_t corner = index / m_cornerPointCount;
  float angle = (index % m_cornerPointCount) * 90.f /
                static_cast<float>(m_cornerPointCount - 1);
  float rad = angle * 3.141592654f / 180.f;

  sf::Vector2f center;
  switch (corner) {
    case 0:
      center = {m_size.x - m_radius, m_radius};
      rad = -rad;
      break;  // top-right
    case 1:
      center = {m_radius, m_radius};
      rad = 180.f * 3.141592654f / 180.f - rad;
      break;  // top-left
    case 2:
      center = {m_radius, m_size.y - m_radius};
      rad = 180.f * 3.141592654f / 180.f + rad;
      break;  // bottom-left
    default:
      center = {m_size.x - m_radius, m_size.y - m_radius};
      rad = 2.f * 3.141592654f - rad;
      break;  // bottom-right
  }

  return {center.x + std::cos(rad) * m_radius,
          center.y + std::sin(rad) * m_radius};
}

}  // namespace lilia::view
