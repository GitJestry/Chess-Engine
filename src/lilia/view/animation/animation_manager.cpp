#include "lilia/view/animation/animation_manager.hpp"

#include <algorithm>

namespace lilia {

void AnimationManager::add(Entity::ID_type entityID, std::unique_ptr<IAnimation> anim) {
  if (m_animations.find(entityID) == m_animations.end()) m_animations[entityID] = std::move(anim);
}

bool AnimationManager::isAnimating(Entity::ID_type entityID) const {
  return m_animations.find(entityID) != m_animations.end();
}

void AnimationManager::endAnim(Entity::ID_type entityID) {
  m_animations.erase(entityID);
}

void AnimationManager::update(float dt) {
  for (auto it = m_animations.begin(); it != m_animations.end();) {
    it->second->update(dt);
    if (it->second->isFinished()) {
      it = m_animations.erase(it);
    } else {
      ++it;
    }
  }
}

void AnimationManager::draw(sf::RenderWindow& window) {
  for (auto& [id, anim] : m_animations) {
    anim->draw(window);
  }
}
}  // namespace lilia
