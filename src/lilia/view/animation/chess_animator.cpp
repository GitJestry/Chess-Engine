#include "lilia/view/animation/chess_animator.hpp"

#include "lilia/view/animation/move_animation.hpp"
#include "lilia/view/animation/piece_placeholder_animation.hpp"
#include "lilia/view/animation/snap_to_square_animation.hpp"
#include "lilia/view/render_constants.hpp"

namespace lilia::view::animation {

ChessAnimator::ChessAnimator(const BoardView& boardRef, PieceManager& pieceMgrRef)
    : m_board_view_ref(boardRef), m_piece_manager_ref(pieceMgrRef) {}

[[nodiscard]] inline Entity::Position mouseToEntityPos(core::MousePos mousePos) {
  return static_cast<Entity::Position>(mousePos);
}

void ChessAnimator::snapAndReturn(core::Square pieceSq, core::MousePos mousePos) {
  m_anim_manager.add(
      m_piece_manager_ref.getPieceID(pieceSq),
      std::make_unique<SnapToSquareAnim>(m_piece_manager_ref, pieceSq, mouseToEntityPos(mousePos),
                                         m_board_view_ref.getSquareScreenPos(pieceSq)));
}
void ChessAnimator::movePiece(core::Square from, core::Square to) {
  m_anim_manager.add(
      m_piece_manager_ref.getPieceID(from),
      std::make_unique<MoveAnim>(m_piece_manager_ref, m_board_view_ref.getSquareScreenPos(from),
                                 m_board_view_ref.getSquareScreenPos(to), from, to));
}
void ChessAnimator::dropPiece(core::Square from, core::Square to) {
  m_piece_manager_ref.movePiece(from, to);
}

void ChessAnimator::piecePlaceHolder(core::Square sq) {
  m_anim_manager.add(m_piece_manager_ref.getPieceID(sq),
                     std::make_unique<PiecePlaceholderAnim>(m_piece_manager_ref, sq));
}
void ChessAnimator::end(core::Square sq) {
  m_anim_manager.endAnim(m_piece_manager_ref.getPieceID(sq));
}

[[nodiscard]] bool ChessAnimator::isAnimating(Entity::ID_type entityID) const {
  return m_anim_manager.isAnimating(entityID);
}
void ChessAnimator::updateAnimations(float dt) {
  m_anim_manager.update(dt);
}

void ChessAnimator::render(sf::RenderWindow& window) {
  m_anim_manager.draw(window);
}

}  // namespace lilia::view::animation
