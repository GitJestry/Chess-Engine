#include "lilia/view/particle_system.hpp"

#include <cmath>
#include <random>

namespace lilia::view {

void ParticleSystem::emitConfetti(const sf::Vector2f &center, float boardSize, std::size_t count) {
  // Seed once per thread instead of every call
  static thread_local std::mt19937 rng{std::random_device{}()};

  std::uniform_real_distribution<float> xDist(center.x - boardSize / 2.f,
                                              center.x + boardSize / 2.f);
  // Give particles a wider horizontal spread and faster upward launch
  std::uniform_real_distribution<float> vxDist(-200.f, 200.f);
  std::uniform_real_distribution<float> vyDist(-1800.f, -1400.f);

  // Wider spread of sizes for more noticeable variation
  std::uniform_real_distribution<float> radiusDist(1.5f, 6.0f);

  float startY = center.y + boardSize / 2.f;

  // Avoid repeated reallocations if you’re emitting a bunch
  if (m_particles.capacity() < m_particles.size() + count) {
    m_particles.reserve(m_particles.size() + count);
  }

  for (std::size_t i = 0; i < count; ++i) {
    float x = xDist(rng);
    float radius = radiusDist(rng);

    sf::CircleShape shape(radius);
    shape.setFillColor(sf::Color::White);  // <-- pure white
    shape.setOrigin(radius, radius);
    shape.setPosition({x, startY});

    sf::Vector2f velocity{vxDist(rng), vyDist(rng)};
    // Longer lifetime so particles have time to fall back down
    m_particles.push_back(Particle{shape, velocity, 4.f, startY});
  }
}

void ParticleSystem::update(float dt) {
  // Simple gravity and slight horizontal jitter to simulate confetti drift
  static constexpr float gravity = 2000.f;  // pixels per second^2
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> jitterDist(-10.f, 10.f);

  for (auto it = m_particles.begin(); it != m_particles.end();) {
    it->lifetime -= dt;
    if (it->lifetime <= 0.f) {
      it = m_particles.erase(it);
      continue;
    }

    // Apply gravity and some horizontal randomness
    it->velocity.y += gravity * dt;
    it->velocity.x += jitterDist(rng) * dt;
    it->shape.move(it->velocity * dt);

    // Remove when particle reaches the bottom of the board again
    if (it->shape.getPosition().y >= it->floorY) {
      it = m_particles.erase(it);
    } else {
      ++it;
    }
  }
}

void ParticleSystem::render(sf::RenderWindow &window) {
  for (auto &p : m_particles) {
    window.draw(p.shape);
  }
}

void ParticleSystem::clear() {
  m_particles.clear();
}

bool ParticleSystem::empty() const {
  return m_particles.empty();
}

}  // namespace lilia::view
