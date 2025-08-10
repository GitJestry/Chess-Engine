#pragma once
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>
#include <memory>

// Wrapper Class

class Entity {
 public:
  explicit Entity(const sf::Texture &texture);
  explicit Entity(sf::Vector2f pos);
  Entity(const sf::Texture &texture, sf::Vector2f pos);
  Entity();
  virtual ~Entity() = default;

  // sets the position of the Sprite internally
  virtual void setPosition(const sf::Vector2f &pos);

  // returns a float Vector of the position, internally returns the sprites position
  sf::Vector2f getPosition() const;

  sf::Vector2f getOriginalSize() const;
  sf::Vector2f getCurrentSize() const;

  // This means whenever the position of this entity or scale will be changed, it always
  // will be based on the center of the sprite
  void setOriginToCenter();

  // this should be implemented by each entity on its own.
  virtual void draw(sf::RenderWindow &window);

  // When you set the texture of a sprite, all it does internally is store a pointer to the texture
  // instance. Therefore, if the texture is destroyed or moves elsewhere in memory, the sprite ends
  // up with an invalid texture pointer. This is why we only take an lvalue referance of a
  // sf::Texture, which is located on the heap by the TextureTable
  void setTexture(const sf::Texture &texture);

  // setSpriteSize modifies the absolute scale, meaning it overrides any previous scaling applied to
  // the sprite the width and height fraction are expressing how the sprite should be scaled for
  // example, width_fraction = 0.5 means 50% of the original width
  void setScale(float width_fraction, float height_fraction);

  // To Specify the Rect used from the spritesheet
  void setTextureRect(sf::IntRect rect);

 private:
  // not protected because every entity class should only operate and make changes to the sprite via
  // member-functions
  sf::Sprite m_sprite;
};
