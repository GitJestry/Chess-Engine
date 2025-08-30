#pragma once

#include <string>
#include <SFML/System/Vector2.hpp>

#include "board.hpp"

namespace lilia::view {

class BoardView {
 public:
  BoardView();

    void init();
    void resize(const sf::Vector2u& windowSize);
    void renderBoard(sf::RenderWindow& window);
    [[nodiscard]] Entity::Position getSquareScreenPos(core::Square sq) const;

 private:
  Board m_board;
};

}  // namespace lilia::view
