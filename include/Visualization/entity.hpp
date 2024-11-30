#pragma once
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <memory>

class Entity {
  public:

    Entity(const sf::Texture& texture);
    Entity(const sf::Texture& texture, sf::Vector2f pos);
    Entity() = default;

    // sets the position of the Sprite internally
    void SetPosition(const sf::Vector2f& pos);

    // returns a float Vector of the position, internally the sprites position
    sf::Vector2f GetPosition();

    // this should be implemented by each entity on its own.
    // The common way will be to reference the board size and resize itself accordingly
    virtual void ResizeSprite(float size) = 0;

    // When you set the texture of a sprite, all it does internally is store a pointer to the texture instance. 
    // Therefore, if the texture is destroyed or moves elsewhere in memory, the sprite ends up with an invalid texture pointer.
    // This is why we only take an lvalue referance of a sf::Texture, which is located on the heap by the TextureTable
    void SetTexture(const sf::Texture& texture);

    // setSpriteSize modifies the absolute scale, meaning it overrides any previous scaling applied to the sprite
    // the width and height fraction are expressing how the sprite should be scaled in percenatage of the original.
    // for example, width_fraction = 0.5 means 50% of the original width
    void SetSpriteSize(float width_fraction, float height_fraction);

    // This method will probably only be used when the window wants to draw the entity(the sprite) on the scene
    const sf::Sprite& GetSprite();

  protected:
    sf::Sprite sprite_{};
};