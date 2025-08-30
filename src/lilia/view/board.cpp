#include "lilia/view/board.hpp"

#include <SFML/Graphics/RenderWindow.hpp>

namespace lilia::view {

Board::Board(Entity::Position pos) : Entity(pos) {}

void Board::init(const sf::Texture &textureWhite, const sf::Texture &textureBlack,
                 const sf::Texture &textureBoard) {
  setTexture(textureBoard);
  setScale(constant::BOARD_PX_SIZE, constant::BOARD_PX_SIZE);

  sf::Vector2f board_offset(
      getPosition().x - constant::BOARD_PX_SIZE / 2 + constant::SQUARE_PX_SIZE / 2,
      getPosition().y - constant::BOARD_PX_SIZE / 2 + constant::SQUARE_PX_SIZE / 2);

  for (int rank = 0; rank < constant::BOARD_SIZE; ++rank) {
    for (int file = 0; file < constant::BOARD_SIZE; ++file) {
      int index = file + rank * constant::BOARD_SIZE;

      float x = board_offset.x + file * constant::SQUARE_PX_SIZE;
      float y = board_offset.y + (constant::BOARD_SIZE - 1 - rank) * constant::SQUARE_PX_SIZE;

      m_squares[index].setPosition({x, y});

      if ((rank + file) % 2 == 0)
        m_squares[index].setTexture(textureBlack);
      else
        m_squares[index].setTexture(textureWhite);
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

  for (auto &s : m_squares) {
    s.draw(window);
  }
}

void Board::setPosition(const Entity::Position &pos) {
  Entity::setPosition(pos);
  Entity::Position board_offset(
      getPosition().x - constant::BOARD_PX_SIZE / 2 + constant::SQUARE_PX_SIZE / 2,
      getPosition().y - constant::BOARD_PX_SIZE / 2 + constant::SQUARE_PX_SIZE / 2);

  for (int rank = 0; rank < constant::BOARD_SIZE; ++rank) {
    for (int file = 0; file < constant::BOARD_SIZE; ++file) {
      int index = file + rank * constant::BOARD_SIZE;

      float x = board_offset.x + file * constant::SQUARE_PX_SIZE;
      float y = board_offset.y + (constant::BOARD_SIZE - 1 - rank) * constant::SQUARE_PX_SIZE;

      m_squares[index].setPosition({x, y});
    }
  }
}

}  // namespace lilia::view
