#pragma once

#include "../../chess_types.hpp"
#include "../piece_manager.hpp"
#include "i_animation.hpp"

namespace lilia {

class MoveAnim : public IAnimation {
 public:
  explicit MoveAnim(PieceManager& pieceMgrRef, Entity::Position s, Entity::Position e,
                    core::Square from = core::Square::NONE, core::Square to = core::Square::NONE);
  void update(float dt) override;
  void draw(sf::RenderWindow& window) override;
  [[nodiscard]] inline bool isFinished() const override;

 private:
  PieceManager& m_piece_manager_ref;
  Entity::Position m_startPos;
  Entity::Position m_endPos;
  float m_elapsed = 0.f;
  float m_duration = core::ANIM_MOVE_SPEED;
  bool m_finish = false;
  core::Square m_from;
  core::Square m_to;
};

}  // namespace lilia
