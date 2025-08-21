#include "lilia/view/board.hpp"

#include <SFML/Graphics/RenderWindow.hpp>

namespace lilia::view {

Board::Board(Entity::Position pos) : Entity(pos) {}

void Board::init(const sf::Texture &textureWhite, const sf::Texture &textureBlack,
                 const sf::Texture &textureBoard) {
  setTexture(textureBoard);
  setScale(constant::WINDOW_PX_SIZE, constant::WINDOW_PX_SIZE);

  sf::Vector2f board_offset(
      getPosition().x - constant::WINDOW_PX_SIZE / 2 + constant::SQUARE_PX_SIZE / 2,
      getPosition().y - constant::WINDOW_PX_SIZE / 2 + constant::SQUARE_PX_SIZE / 2);

  for (int rank = 0; rank < constant::BOARD_SIZE; ++rank) {    // rank 0 = 1st rank (bottom)
    for (int file = 0; file < constant::BOARD_SIZE; ++file) {  // file 0 = 'a' file (left)

      // Stockfish index: file + rank * 8
      int index = file + rank * constant::BOARD_SIZE;

      // Convert to SFML coordinates (y inverted if origin is top-left)
      float x = board_offset.x + file * constant::SQUARE_PX_SIZE;
      float y = board_offset.y + (constant::BOARD_SIZE - 1 - rank) * constant::SQUARE_PX_SIZE;

      m_squares[index].setPosition({x, y});

      // Alternate colors
      if ((rank + file) % 2 == 0)
        m_squares[index].setTexture(textureWhite);
      else
        m_squares[index].setTexture(textureBlack);
      m_squares[index].setScale(constant::SQUARE_PX_SIZE, constant::SQUARE_PX_SIZE);
      m_squares[index].setOriginToCenter();
    }
  }
}

[[nodiscard]] Entity::Position Board::getPosOfSquare(core::Square sq) const {
  return m_squares[static_cast<size_t>(sq)].getPosition();
}

void Board::draw(sf::RenderWindow &window) {
  Entity::draw(window);
  // Draw the board
  for (auto &s : m_squares) {
    s.draw(window);
  }
}

void Board::setPosition(const Entity::Position &pos) {
  Entity::setPosition(pos);
  Entity::Position board_offset(
      getPosition().x - constant::WINDOW_PX_SIZE / 2 + constant::SQUARE_PX_SIZE / 2,
      getPosition().y - constant::WINDOW_PX_SIZE / 2 + constant::SQUARE_PX_SIZE / 2);

  for (int rank = 0; rank < constant::BOARD_SIZE; ++rank) {    // rank 0 = 1st rank (bottom)
    for (int file = 0; file < constant::BOARD_SIZE; ++file) {  // file 0 = 'a' file (left)

      // Stockfish index: file + rank * 8
      int index = file + rank * constant::BOARD_SIZE;

      // Convert to SFML coordinates (y inverted if origin is top-left)
      float x = board_offset.x + file * constant::SQUARE_PX_SIZE;
      float y = board_offset.y + (constant::BOARD_SIZE - 1 - rank) * constant::SQUARE_PX_SIZE;

      m_squares[index].setPosition({x, y});
    }
  }
}

}  // namespace lilia::view
