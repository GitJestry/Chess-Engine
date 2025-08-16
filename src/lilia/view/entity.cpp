#include "lilia/view/entity.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <atomic>

namespace lilia {

void Entity::setPosition(const Position &pos) {
  m_sprite.setPosition(pos);
}

Entity::Position Entity::getPosition() const {
  return m_sprite.getPosition();
}

void Entity::setTexture(const sf::Texture &texture) {
  m_sprite.setTexture(texture);
}

const sf::Texture &Entity::getTexture() const {
  return *m_sprite.getTexture();
}

void Entity::setScale(float widthFraction, float heightFraction) {
  m_sprite.setScale(widthFraction, heightFraction);
}

void Entity::setOriginToCenter() {
  Position bounds = getOriginalSize();                 // Get the local bounds of the sprite
  m_sprite.setOrigin(bounds.x / 2.f, bounds.y / 2.f);  // Set the origin to the center
}

Entity::Entity(const sf::Texture &texture) : m_id(generateId()), m_sprite(texture) {
  setOriginToCenter();
}

Entity::Entity() : m_id(generateId()), m_sprite() {
  setOriginToCenter();
}
Entity::Entity(Position pos) : Entity() {
  m_sprite.setPosition(pos);
}

Entity::ID_type Entity::getId() const {
  return m_id;
}

Entity::Entity(const sf::Texture &texture, Position pos) : m_id(generateId()), m_sprite(texture) {
  setOriginToCenter();
  m_sprite.setPosition(pos);
}
Entity::Position Entity::getOriginalSize() const {
  return Position(m_sprite.getLocalBounds().width, m_sprite.getLocalBounds().height);
}
Entity::Position Entity::getCurrentSize() const {
  return Position(m_sprite.getGlobalBounds().width, m_sprite.getGlobalBounds().height);
}

void Entity::draw(sf::RenderWindow &window) {
  window.draw(m_sprite);
}

}  // namespace lilia
