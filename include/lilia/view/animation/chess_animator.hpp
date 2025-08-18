#pragma once

#include "../../core_types.hpp"
#include "../board_view.hpp"
#include "../piece_manager.hpp"
#include "../render_types.hpp"
#include "animation_manager.hpp"

namespace lilia {

class ChessAnimator {
 public:
  ChessAnimator(const BoardView& boardRef, PieceManager& pieceMgrRef);

  void snapAndReturn(core::Square pieceSq, core::MousePos mousePos);
  void movePiece(core::Square from, core::Square to);
  void dropPiece(core::Square from, core::Square to);
  void piecePlaceHolder(core::Square sq);
  void end(core::Square sq);

  [[nodiscard]] bool isAnimating(Entity::ID_type entityID) const;
  void updateAnimations(float dt);
  void render(sf::RenderWindow& window);

 private:
  const BoardView& m_board_view_ref;
  PieceManager& m_piece_manager_ref;
  AnimationManager m_anim_manager;
};

}  // namespace lilia
