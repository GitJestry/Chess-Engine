#pragma once

#include <memory>
#include <unordered_map>

#include "../entity.hpp"
#include "i_animation.hpp"

namespace lilia::view::animation {

class AnimationManager {
 public:
  AnimationManager() = default;

  void add(Entity::ID_type entityID, std::unique_ptr<IAnimation> anim);

  void declareHighlightLevel(Entity::ID_type entityID);
  void endAnim(Entity::ID_type entityID);
  [[nodiscard]] bool isAnimating(Entity::ID_type entityID) const;
  void update(float dt);
  void draw(sf::RenderWindow& window);
  void highlightLevelDraw(sf::RenderWindow& window);

 private:
  std::unordered_map<Entity::ID_type, std::unique_ptr<IAnimation>> m_highlight_level_animations;
  std::unordered_map<Entity::ID_type, std::unique_ptr<IAnimation>> m_animations;
};

}  
