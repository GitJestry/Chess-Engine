#include "lilia/view/animation/animation_manager.hpp"

#include <algorithm>
#include <iostream>

namespace lilia::view::animation {

void AnimationManager::add(Entity::ID_type entityID, std::unique_ptr<IAnimation> anim) {
  if (m_animations.find(entityID) == m_animations.end()) {
    m_animations[entityID] = std::move(anim);
  }
}

[[nodiscard]] bool AnimationManager::isAnimating(Entity::ID_type entityID) const {
  return m_animations.find(entityID) != m_animations.end();
}

void AnimationManager::declareHighlightLevel(Entity::ID_type entityID) {
  auto it = m_animations.find(entityID);
  if (it != m_animations.end()) {
    m_highlight_level_animations[entityID] = std::move(it->second);
    m_animations.erase(it);
  }
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
  for (auto it = m_highlight_level_animations.begin(); it != m_highlight_level_animations.end();) {
    it->second->update(dt);
    if (it->second->isFinished()) {
      it = m_highlight_level_animations.erase(it);
    } else {
      ++it;
    }
  }
}

void AnimationManager::draw(sf::RenderWindow& window) {
  for (auto& [id, anim] : m_animations) anim->draw(window);
}

void AnimationManager::highlightLevelDraw(sf::RenderWindow& window) {
  for (auto& [id, anim] : m_highlight_level_animations) anim->draw(window);
}

}  
