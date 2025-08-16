#include "lilia/view/animation/placeholder_animation.hpp"

namespace lilia {

PlaceholderAnim::PlaceholderAnim(Entity& eref) : m_entity_ref(eref) {}
void PlaceholderAnim::update(float dt) {}
void PlaceholderAnim::draw(sf::RenderWindow& window) {
  m_entity_ref.draw(window);
}
bool PlaceholderAnim::isFinished() const {
  return false;
}

};  // namespace lilia
