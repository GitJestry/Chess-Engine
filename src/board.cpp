#include <board.hpp>

Board::Board() : Entity() {}
Board::Board(sf::Vector2f pos) : Entity() { setPosition(pos); }

void Board::initialize(sf::Texture &texture_white, sf::Texture &texture_black, sf::Texture &texture_board, float board_size)
{
  setTexture(texture_board);
  setScale(board_size, board_size);
  float square_size = board_size / 8;

  sf::Vector2f board_left_top_corner(getPosition().x - board_size / 2 + square_size / 2, getPosition().y - board_size / 2 + square_size / 2);

  for (int row = 0; row < 8; ++row)
  {
    for (int col = 0; col < 8; ++col)
    {
      // Calculate the index for the 1D array
      int index = row * 8 + col;
      // Set position
      sf::Vector2f new_position(board_left_top_corner.x + col * square_size, board_left_top_corner.y + row * square_size);
      this->squares_[index].setPosition(new_position);

      // Alternate colors
      if ((row + col) % 2 == 0)
        this->squares_[index].setTexture(texture_white);
      else
        this->squares_[index].setTexture(texture_black);
      this->squares_[index].setScale(square_size, square_size);
    }
  }
}

std::array<Entity, 64> &Board::getSquares()
{
  return this->squares_;
}

void Board::draw(sf::RenderWindow &window)
{
  setOriginToCenter();
  Entity::draw(window);
  // Draw the board
  for (auto &s : squares_)
  {
    s.setOriginToCenter();
    s.draw(window);
  }
}

void Board::setPosition(const sf::Vector2f &pos)
{
  Entity::setPosition(pos);
  float board_size = getCurrentSize().x;
  float square_size = board_size / 8;
  sf::Vector2f board_offset(getPosition().x - board_size / 2 + square_size / 2, getPosition().y - board_size / 2 + square_size / 2);

  for (int row = 0; row < 8; ++row)
  {
    for (int col = 0; col < 8; ++col)
    {
      // Calculate the index for the 1D array
      int index = row * 8 + col;
      // Set position
      sf::Vector2f new_position(board_offset.x + col * square_size, board_offset.y + row * square_size);
      squares_[index].setPosition(new_position);
    }
  }
}
