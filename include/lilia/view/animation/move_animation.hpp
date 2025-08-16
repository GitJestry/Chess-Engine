#pragma once

#include <functional>

#include "../../chess_types.hpp"
#include "../piece.hpp"
#include "i_animation.hpp"

namespace lilia {

class MoveAnim : public IAnimation {
 public:
  using MoveFunction = std::function<void(core::Square, core::Square)>;

  explicit MoveAnim(Piece& piece, Entity::Position s, Entity::Position e, float dur,
                    MoveFunction func = nullptr, core::Square from = core::Square::NONE,
                    core::Square to = core::Square::NONE);
  void update(float dt) override;
  void draw(sf::RenderWindow& window) override;
  bool isFinished() const override;

 private:
  Piece& m_piece_ref;
  Entity::Position m_startPos;
  Entity::Position m_endPos;
  float m_elapsed = 0.f;
  float m_duration = 0.1f;  // 100ms
  bool m_finish = false;
  MoveFunction m_move_func;
  core::Square m_from;
  core::Square m_to;
};

}  // namespace lilia
