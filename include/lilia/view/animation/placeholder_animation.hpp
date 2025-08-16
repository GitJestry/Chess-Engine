#pragma once

#include "../entity.hpp"
#include "i_animation.hpp"

namespace lilia {

class PlaceholderAnim : public IAnimation {
 public:
  explicit PlaceholderAnim(Entity& eref);
  void update(float dt) override;
  void draw(sf::RenderWindow& window) override;
  bool isFinished() const override;

 private:
  Entity& m_entity_ref;
};

}  // namespace lilia
