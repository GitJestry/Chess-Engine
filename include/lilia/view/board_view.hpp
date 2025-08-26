#pragma once

#include <string>

#include "board.hpp"

namespace lilia::view {

class BoardView {
 public:
  BoardView();

  void init();
  void renderBoard(sf::RenderWindow& window);
  [[nodiscard]] Entity::Position getSquareScreenPos(core::Square sq) const;

 private:
  Board m_board;
};

}  // namespace lilia::view
