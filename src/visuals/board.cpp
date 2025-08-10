#include "visuals/board.hpp"

Board::Board() : Entity() {}
Board::Board(sf::Vector2f pos) : Entity() {
  setPosition(pos);
}

void Board::init(sf::Texture &texture_white, sf::Texture &texture_black,
                 sf::Texture &texture_board) {
  setTexture(texture_board);
  setScale(WINDOW_SIZE, WINDOW_SIZE);

  sf::Vector2f board_offset(getPosition().x - WINDOW_SIZE / 2 + SQUARE_SIZE / 2,
                            getPosition().y - WINDOW_SIZE / 2 + SQUARE_SIZE / 2);

  for (int rank = 0; rank < BOARD_SIZE; ++rank) {    // rank 0 = 1st rank (bottom)
    for (int file = 0; file < BOARD_SIZE; ++file) {  // file 0 = 'a' file (left)

      // Stockfish index: file + rank * 8
      int index = file + rank * BOARD_SIZE;

      // Convert to SFML coordinates (y inverted if origin is top-left)
      float x = board_offset.x + file * SQUARE_SIZE;
      float y = board_offset.y + (BOARD_SIZE - 1 - rank) * SQUARE_SIZE;

      m_squares[index].setPosition({x, y});

      // Alternate colors
      if ((rank + file) % 2 == 0)
        m_squares[index].setTexture(texture_white);
      else
        m_squares[index].setTexture(texture_black);
      m_squares[index].setScale(SQUARE_SIZE, SQUARE_SIZE);
    }
  }
}

std::array<Entity, BOARD_SIZE * BOARD_SIZE> &Board::getSquares() {
  return m_squares;
}

#include <iostream>

void Board::draw(sf::RenderWindow &window) {
  setOriginToCenter();
  Entity::draw(window);
  // Draw the board
  for (auto &s : m_squares) {
    s.setOriginToCenter();
    s.draw(window);
  }
}

#include <iostream>

void Board::setPosition(const sf::Vector2f &pos) {
  Entity::setPosition(pos);
  sf::Vector2f board_offset(getPosition().x - WINDOW_SIZE / 2 + SQUARE_SIZE / 2,
                            getPosition().y - WINDOW_SIZE / 2 + SQUARE_SIZE / 2);

  for (int rank = 0; rank < BOARD_SIZE; ++rank) {    // rank 0 = 1st rank (bottom)
    for (int file = 0; file < BOARD_SIZE; ++file) {  // file 0 = 'a' file (left)

      // Stockfish index: file + rank * 8
      int index = file + rank * BOARD_SIZE;

      // Convert to SFML coordinates (y inverted if origin is top-left)
      float x = board_offset.x + file * SQUARE_SIZE;
      float y = board_offset.y + (BOARD_SIZE - 1 - rank) * SQUARE_SIZE;

      m_squares[index].setPosition({x, y});
    }
  }
}
