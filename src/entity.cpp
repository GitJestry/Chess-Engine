#include <Entity.hpp>
#include <stdexcept>

void Entity::setPosition(const sf::Vector2f &pos) { this->sprite_.setPosition(pos); }

sf::Vector2f Entity::getPosition() const { return this->sprite_.getPosition(); }

void Entity::setTexture(const sf::Texture &texture)
{
  this->sprite_.setTexture(texture);
}

void Entity::setScale(float width_fraction, float height_fraction)
{
  this->sprite_.setScale(width_fraction, height_fraction);
}

void Entity::setOriginToCenter()
{
  sf::Vector2f bounds = getOriginalSize();           // Get the local bounds of the sprite
  sprite_.setOrigin(bounds.x / 2.f, bounds.y / 2.f); // Set the origin to the center
}

Entity::Entity(const sf::Texture &texture) : sprite_(texture)
{
  setOriginToCenter();
}

Entity::Entity() : sprite_() { setOriginToCenter(); }
Entity::Entity(sf::Vector2f pos) : Entity() { this->sprite_.setPosition(pos); }

Entity::Entity(const sf::Texture &texture, sf::Vector2f pos) : sprite_(texture)
{
  setOriginToCenter();
  this->sprite_.setPosition(pos);
}
sf::Vector2f Entity::getOriginalSize() const
{
  return sf::Vector2f(
      sprite_.getLocalBounds().width,
      sprite_.getLocalBounds().height);
}
sf::Vector2f Entity::getCurrentSize() const
{
  return sf::Vector2f(
      sprite_.getGlobalBounds().width,
      sprite_.getGlobalBounds().height);
}

void Entity::draw(sf::RenderWindow &window)
{
  window.draw(this->sprite_);
}
