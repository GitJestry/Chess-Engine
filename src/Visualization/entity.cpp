#include <Entity.hpp>

void Entity::SetPosition(const sf::Vector2f& position) { this->sprite_.setPosition(position);}

sf::Vector2f Entity::GetPosition() { return this->sprite_.getPosition(); }

void Entity::SetTexture(const sf::Texture& texture) {this->sprite_.setTexture(texture);}

void Entity::SetSpriteSize(float width_fraction, float height_fraction) { this->sprite_.setScale(width_fraction, height_fraction); }

const sf::Sprite& Entity::GetSprite() { return this->sprite_; }