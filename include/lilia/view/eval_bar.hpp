#pragma once
#include "entity.hpp"
#include <SFML/Graphics/Text.hpp>
#include <string>

namespace sf {
class RenderWindow;
}

namespace lilia::view {

class EvalBar : Entity {
 public:
  EvalBar();

  virtual void setPosition(const Entity::Position &pos) override;
  void render(sf::RenderWindow &window);
  void update(int eval);
  void setResult(const std::string &result);

 private:
  void scaleToEval(float e);
  Entity m_black_background;
  Entity m_white_fill_eval;
  sf::Font m_font;
  sf::Text m_score_text;
  float m_display_eval{0.f};
  float m_target_eval{0.f};
  std::string m_result;
};

}  // namespace lilia::view
