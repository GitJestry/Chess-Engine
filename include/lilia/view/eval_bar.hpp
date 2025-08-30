#pragma once
#include "entity.hpp"

namespace sf {
class RenderWindow;
}

namespace lilia::view {

class EvalBar : public Entity {
 public:
  EvalBar();

  virtual void setPosition(const Entity::Position &pos) override;
  void setScale(float width, float height);
  void render(sf::RenderWindow &window);
  void update(int eval);

 private:
  void scaleToEval(int e);
  Entity m_black_background;
  Entity m_white_fill_eval;
};

}  // namespace lilia::view
