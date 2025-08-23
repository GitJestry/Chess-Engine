#include "lilia/view/animation/warning_animation.hpp"

#include <cmath>

#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view::animation {

WarningAnim::WarningAnim(Entity::Position ksqPos) : m_warning_highlight(ksqPos) {
  m_warning_highlight.setTexture(
      TextureTable::getInstance().get(constant::STR_TEXTURE_WARNINGHLIGHT));
  m_warning_highlight.setOriginToCenter();
  m_warning_highlight.setScale(constant::SQUARE_PX_SIZE, constant::SQUARE_PX_SIZE);
}

void WarningAnim::update(float dt) {
  if (m_finish) return;

  m_elapsed += dt;

  if (m_elapsed >= m_totalDuration) {
    m_finish = true;
    return;
  }

  float phase = std::fmod(m_elapsed, m_blinkPeriod * 2.f);
  if (phase < m_blinkPeriod) {
    m_warning_highlight.setTexture(
        TextureTable::getInstance().get(constant::STR_TEXTURE_WARNINGHLIGHT));
  } else {
    m_warning_highlight.setTexture(
        TextureTable::getInstance().get(constant::STR_TEXTURE_TRANSPARENT));
  }
}

void WarningAnim::draw(sf::RenderWindow& window) {
  m_warning_highlight.draw(window);
}
[[nodiscard]] bool WarningAnim::isFinished() const {
  return m_finish;
}

}  // namespace lilia::view::animation
