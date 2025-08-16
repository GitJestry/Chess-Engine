#include "lilia/view/animation/move_animation.hpp"

namespace lilia {

MoveAnim::MoveAnim(Piece& piece, Entity::Position s, Entity::Position e, float dur,
                   MoveFunction func, core::Square from, core::Square to)
    : m_piece_ref(piece),
      m_startPos(s),
      m_endPos(e),
      m_duration(dur),
      m_move_func(func),
      m_from(from),
      m_to(to) {}

void MoveAnim::update(float dt) {
  m_elapsed += dt;
  float t = std::min(m_elapsed / m_duration, 1.f);
  Entity::Position pos = m_startPos + t * (m_endPos - m_startPos);
  m_piece_ref.setPosition(pos);

  if (t >= 1.f) {
    m_finish = true;
    m_move_func(m_from, m_to);
  }
}

void MoveAnim::draw(sf::RenderWindow& window) {
  m_piece_ref.draw(window);
}

bool MoveAnim::isFinished() const {
  return m_finish;
}

};  // namespace lilia
