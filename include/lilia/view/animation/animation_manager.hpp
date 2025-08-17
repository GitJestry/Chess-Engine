#pragma once

#include <memory>
#include <unordered_map>

#include "../entity.hpp"
#include "i_animation.hpp"

namespace lilia {

class AnimationManager {
 public:
  AnimationManager() = default;

  void add(Entity::ID_type entityID, std::unique_ptr<IAnimation> anim);
  void endAnim(Entity::ID_type entityID);
  [[nodiscard]] bool isAnimating(Entity::ID_type entityID) const;
  void update(float dt);
  void draw(sf::RenderWindow& window);

 private:
  std::unordered_map<Entity::ID_type, std::unique_ptr<IAnimation>> m_animations;
};

}  // namespace lilia
