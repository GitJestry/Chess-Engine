#include "lilia/view/particle_system.hpp"

#include <cmath>
#include <random>

namespace lilia::view {

void ParticleSystem::emitConfetti(const sf::Vector2f &center, float boardSize,
                                  std::size_t count) {
  std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<float> xDist(center.x - boardSize / 2.f,
                                              center.x + boardSize / 2.f);
  std::uniform_real_distribution<float> vxDist(-50.f, 50.f);
  std::uniform_real_distribution<float> vyDist(-250.f, -150.f);
  std::uniform_real_distribution<float> radiusDist(2.f, 4.f);
  std::uniform_int_distribution<int> colorDist(0, 255);

  float startY = center.y + boardSize / 2.f;
  for (std::size_t i = 0; i < count; ++i) {
    float x = xDist(rng);
    float radius = radiusDist(rng);
    sf::CircleShape shape(radius);
    shape.setFillColor(
        sf::Color(colorDist(rng), colorDist(rng), colorDist(rng)));
    shape.setOrigin(radius, radius);
    shape.setPosition({x, startY});
    sf::Vector2f velocity{vxDist(rng), vyDist(rng)};
    Particle p{shape, velocity, 2.f};
    m_particles.push_back(p);
  }
}

void ParticleSystem::update(float dt) {
  for (auto it = m_particles.begin(); it != m_particles.end();) {
    it->lifetime -= dt;
    if (it->lifetime <= 0.f) {
      it = m_particles.erase(it);
    } else {
      it->shape.move(it->velocity * dt);
      ++it;
    }
  }
}

void ParticleSystem::render(sf::RenderWindow &window) {
  for (auto &p : m_particles) {
    window.draw(p.shape);
  }
}

void ParticleSystem::clear() { m_particles.clear(); }

bool ParticleSystem::empty() const { return m_particles.empty(); }

} // namespace lilia::view
