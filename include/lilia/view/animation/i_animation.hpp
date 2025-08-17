#pragma once

// forward decleration
namespace sf {
class RenderWindow;
}

namespace lilia {

class IAnimation {
 public:
  virtual ~IAnimation() = default;
  virtual void update(float dt) = 0;
  virtual void draw(sf::RenderWindow& window) = 0;
  [[nodiscard]] virtual bool isFinished() const = 0;
};

}  // namespace lilia
