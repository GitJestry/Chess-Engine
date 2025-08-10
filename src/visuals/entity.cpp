#include "visuals/entity.hpp"

#include <stdexcept>

void Entity::setPosition(const sf::Vector2f &pos) {
  m_sprite.setPosition(pos);
}

sf::Vector2f Entity::getPosition() const {
  return m_sprite.getPosition();
}

void Entity::setTexture(const sf::Texture &texture) {
  m_sprite.setTexture(texture);
}

sf::Texture Entity::getTexture() {
  return *m_sprite.getTexture();
}

void Entity::setScale(float width_fraction, float height_fraction) {
  m_sprite.setScale(width_fraction, height_fraction);
}

void Entity::setOriginToCenter() {
  sf::Vector2f bounds = getOriginalSize();             // Get the local bounds of the sprite
  m_sprite.setOrigin(bounds.x / 2.f, bounds.y / 2.f);  // Set the origin to the center
}

Entity::Entity(const sf::Texture &texture) : m_sprite(texture) {
  setOriginToCenter();
}

Entity::Entity() : m_sprite() {
  setOriginToCenter();
}
Entity::Entity(sf::Vector2f pos) : Entity() {
  m_sprite.setPosition(pos);
}

Entity::Entity(const sf::Texture &texture, sf::Vector2f pos) : m_sprite(texture) {
  setOriginToCenter();
  m_sprite.setPosition(pos);
}
sf::Vector2f Entity::getOriginalSize() const {
  return sf::Vector2f(m_sprite.getLocalBounds().width, m_sprite.getLocalBounds().height);
}
sf::Vector2f Entity::getCurrentSize() const {
  return sf::Vector2f(m_sprite.getGlobalBounds().width, m_sprite.getGlobalBounds().height);
}

void Entity::draw(sf::RenderWindow &window) {
  window.draw(m_sprite);
}
