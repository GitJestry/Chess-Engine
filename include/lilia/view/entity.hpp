#pragma once

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/System/Vector2.hpp>
#include <atomic>

// forward decleration
namespace sf {
class RenderWindow;
}

namespace lilia::view {

/**
 * @brief Wrapper Class of sf::Sprite
 */

class Entity {
 public:
  using Position = sf::Vector2f;
  using ID_type = size_t;

  Entity(const sf::Texture &texture);
  Entity(Position pos);
  Entity(const sf::Texture &texture, Position pos);
  Entity();
  virtual ~Entity() = default;

  virtual void setPosition(const Position &pos);

  [[nodiscard]] Position getPosition() const;

  // The Textures Original Size
  [[nodiscard]] Position getOriginalSize() const;

  // The current Textures size
  [[nodiscard]] Position getCurrentSize() const;

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

  [[nodiscard]] const sf::Texture &getTexture() const;

  // setSpriteSize modifies the absolute scale, meaning it overrides any previous scaling applied to
  // the sprite the width and height fraction are expressing how the sprite should be scaled for
  // example, width_fraction = 0.5 means 50% of the original width
  void setScale(float widthFraction, float heightFraction);

  [[nodiscard]] ID_type getId() const;

 private:
  ID_type m_id;

  // static ID Counter. Counts upwards for every new entity, beginning with 0
  // guarantees no identical EntityIDs
  [[nodiscard]] static ID_type generateId() {
    static std::atomic_size_t counter{1};
    return counter.fetch_add(1, std::memory_order_relaxed);
  }

  // not protected because every entity class should only operate and make changes to the sprite via
  // member-functions
  sf::Sprite m_sprite;
};

}  // namespace lilia::view
