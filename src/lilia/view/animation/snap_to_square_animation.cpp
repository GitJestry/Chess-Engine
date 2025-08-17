#include "lilia/view/animation/snap_to_square_animation.hpp"

namespace lilia {

SnapToSquareAnim::SnapToSquareAnim(PieceManager& pieceMgrRef, core::Square pieceSq,
                                   Entity::Position s, Entity::Position e)
    : m_piece_manager_ref(pieceMgrRef), m_piece_square(pieceSq), m_startPos(s), m_endPos(e) {}
void SnapToSquareAnim::update(float dt) {
  m_elapsed += dt;
  float t = std::min(m_elapsed / m_duration, 1.f);
  Entity::Position pos = m_startPos + t * (m_endPos - m_startPos);
  m_piece_manager_ref.setPieceToScreenPos(m_piece_square, pos);

  if (t >= 1.f) {
    m_finish = true;
  }
}
void SnapToSquareAnim::draw(sf::RenderWindow& window) {
  m_piece_manager_ref.renderPiece(m_piece_square, window);
}
[[nodiscard]] inline bool SnapToSquareAnim::isFinished() const {
  return m_finish;
}

};  // namespace lilia
