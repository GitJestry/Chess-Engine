#pragma once

#include "../../chess_types.hpp"
#include "../../core_types.hpp"
#include "../piece.hpp"
#include "i_animation.hpp"

namespace lilia {

class SnapToSquareAnim : public IAnimation {
 public:
  explicit SnapToSquareAnim(Piece& piece, Entity::Position s, Entity::Position e, float dur);
  void update(float dt) override;
  void draw(sf::RenderWindow& window) override;
  bool isFinished() const override;

 private:
  Piece& m_piece_ref;
  Entity::Position m_startPos;
  Entity::Position m_endPos;
  float m_elapsed = 0.f;
  float m_duration = .1f;  // 100ms
  bool m_finish = false;
};

}  // namespace lilia
