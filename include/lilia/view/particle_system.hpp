#pragma once

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>
#include <vector>

namespace lilia::view {

class ParticleSystem {
public:
  struct Particle {
    sf::CircleShape shape;
    sf::Vector2f velocity;
    float lifetime;
    float floorY; // y position where particle should disappear
    float totalLifetime;
    bool falling;
    float phase; // random phase for per-particle wiggle
  };

  // Emit confetti across the window starting from the bottom edge.
  void emitConfetti(const sf::Vector2f &center, const sf::Vector2f &windowSize,
                    std::size_t count);
  void update(float dt);
  void render(sf::RenderWindow &window);
  void clear();
  [[nodiscard]] bool empty() const;

private:
  std::vector<Particle> m_particles;
};

} // namespace lilia::view
