#pragma once

#include "../../core_types.hpp"
#include "../piece_manager.hpp"
#include "../render_types.hpp"
#include "i_animation.hpp"

namespace lilia {

class SnapToSquareAnim : public IAnimation {
 public:
  explicit SnapToSquareAnim(PieceManager& pieceMgrRef, core::Square pieceSq, Entity::Position s,
                            Entity::Position e);
  void update(float dt) override;
  void draw(sf::RenderWindow& window) override;
  [[nodiscard]] inline bool isFinished() const override;

 private:
  PieceManager& m_piece_manager_ref;
  core::Square m_piece_square;
  Entity::Position m_startPos;
  Entity::Position m_endPos;
  float m_elapsed = 0.f;
  float m_duration = core::ANIM_SNAP_SPEED;
  bool m_finish = false;
};

}  // namespace lilia
