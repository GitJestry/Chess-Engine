#pragma once

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

#include "entity.hpp"
#include "render_constants.hpp"

namespace lilia::view {

// Simple evaluation bar similar to chess.com. The bar displays the
// evaluation from White's perspective: more white at the bottom means an
// advantage for White.
class EvalBar : public Entity {
 public:
  EvalBar();

  void setEval(int evalCp);
  void draw(sf::RenderWindow& window) override;

 private:
  int m_eval_cp{};
  sf::RectangleShape m_background;
  sf::RectangleShape m_white_part;
};

}  // namespace lilia::view
