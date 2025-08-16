#include "lilia/view/animation/snap_to_square_animation.hpp"

namespace lilia {

SnapToSquareAnim::SnapToSquareAnim(Piece& piece, Entity::Position s, Entity::Position e, float dur)
    : m_piece_ref(piece), m_startPos(s), m_endPos(e), m_duration(dur) {}
void SnapToSquareAnim::update(float dt) {
  m_elapsed += dt;
  float t = std::min(m_elapsed / m_duration, 1.f);
  Entity::Position pos = m_startPos + t * (m_endPos - m_startPos);
  m_piece_ref.setPosition(pos);

  if (t >= 1.f) {
    m_finish = true;
  }
}
void SnapToSquareAnim::draw(sf::RenderWindow& window) {
  m_piece_ref.draw(window);
}
bool SnapToSquareAnim::isFinished() const {
  return m_finish;
}

};  // namespace lilia
