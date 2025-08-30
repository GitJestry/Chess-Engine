#include "lilia/view/eval_bar.hpp"

#include <algorithm>

namespace lilia::view {

EvalBar::EvalBar() {
  m_background.setSize({static_cast<float>(constant::EVAL_BAR_PX_WIDTH),
                        static_cast<float>(constant::BOARD_PX_SIZE)});
  m_background.setFillColor(sf::Color(50, 50, 50));
  m_background.setPosition(static_cast<float>(constant::BOARD_OFFSET_X -
                                              constant::EVAL_BAR_PX_WIDTH),
                           0.f);

  m_white_part.setSize({static_cast<float>(constant::EVAL_BAR_PX_WIDTH),
                        static_cast<float>(constant::BOARD_PX_SIZE) / 2.f});
  m_white_part.setFillColor(sf::Color::White);
  m_white_part.setPosition(
      static_cast<float>(constant::BOARD_OFFSET_X - constant::EVAL_BAR_PX_WIDTH),
      static_cast<float>(constant::BOARD_PX_SIZE) / 2.f);
}

void EvalBar::setEval(int evalCp) {
  m_eval_cp = evalCp;
  const int clamped = std::clamp(evalCp, -1000, 1000);
  const float ratio = (clamped + 1000.f) / 2000.f;
  const float whiteHeight = ratio * static_cast<float>(constant::BOARD_PX_SIZE);
  m_white_part.setSize({static_cast<float>(constant::EVAL_BAR_PX_WIDTH), whiteHeight});
  m_white_part.setPosition(
      static_cast<float>(constant::BOARD_OFFSET_X - constant::EVAL_BAR_PX_WIDTH),
      static_cast<float>(constant::BOARD_PX_SIZE) - whiteHeight);
}

void EvalBar::draw(sf::RenderWindow& window) {
  window.draw(m_background);
  window.draw(m_white_part);
}

}  // namespace lilia::view
