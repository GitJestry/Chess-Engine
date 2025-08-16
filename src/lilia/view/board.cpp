#include "lilia/view/board.hpp"

#include <SFML/Graphics/RenderWindow.hpp>

namespace lilia {

Board::Board(Entity::Position pos) : Entity(pos) {}

void Board::init(const sf::Texture &textureWhite, const sf::Texture &textureBlack,
                 const sf::Texture &textureBoard) {
  setTexture(textureBoard);
  setScale(core::WINDOW_PX_SIZE, core::WINDOW_PX_SIZE);

  sf::Vector2f board_offset(getPosition().x - core::WINDOW_PX_SIZE / 2 + core::SQUARE_PX_SIZE / 2,
                            getPosition().y - core::WINDOW_PX_SIZE / 2 + core::SQUARE_PX_SIZE / 2);

  for (int rank = 0; rank < core::BOARD_SIZE; ++rank) {    // rank 0 = 1st rank (bottom)
    for (int file = 0; file < core::BOARD_SIZE; ++file) {  // file 0 = 'a' file (left)

      // Stockfish index: file + rank * 8
      int index = file + rank * core::BOARD_SIZE;

      // Convert to SFML coordinates (y inverted if origin is top-left)
      float x = board_offset.x + file * core::SQUARE_PX_SIZE;
      float y = board_offset.y + (core::BOARD_SIZE - 1 - rank) * core::SQUARE_PX_SIZE;

      m_squares[index].setPosition({x, y});

      // Alternate colors
      if ((rank + file) % 2 == 0)
        m_squares[index].setTexture(textureWhite);
      else
        m_squares[index].setTexture(textureBlack);
      m_squares[index].setScale(core::SQUARE_PX_SIZE, core::SQUARE_PX_SIZE);
    }
  }
}

Entity::Position Board::getPosOfSquare(core::Square sq) const {
  return m_squares[static_cast<size_t>(sq)].getPosition();
}

void Board::draw(sf::RenderWindow &window) {
  setOriginToCenter();
  Entity::draw(window);
  // Draw the board
  for (auto &s : m_squares) {
    s.setOriginToCenter();
    s.draw(window);
  }
}

void Board::setPosition(const Entity::Position &pos) {
  Entity::setPosition(pos);
  Entity::Position board_offset(
      getPosition().x - core::WINDOW_PX_SIZE / 2 + core::SQUARE_PX_SIZE / 2,
      getPosition().y - core::WINDOW_PX_SIZE / 2 + core::SQUARE_PX_SIZE / 2);

  for (int rank = 0; rank < core::BOARD_SIZE; ++rank) {    // rank 0 = 1st rank (bottom)
    for (int file = 0; file < core::BOARD_SIZE; ++file) {  // file 0 = 'a' file (left)

      // Stockfish index: file + rank * 8
      int index = file + rank * core::BOARD_SIZE;

      // Convert to SFML coordinates (y inverted if origin is top-left)
      float x = board_offset.x + file * core::SQUARE_PX_SIZE;
      float y = board_offset.y + (core::BOARD_SIZE - 1 - rank) * core::SQUARE_PX_SIZE;

      m_squares[index].setPosition({x, y});
    }
  }
}

}  // namespace lilia
