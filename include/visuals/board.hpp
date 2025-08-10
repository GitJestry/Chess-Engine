#pragma once
#include <SFML/Window/Window.hpp>
#include <array>
#include <constants.hpp>

#include "./entity.hpp"

class Board : public Entity {
 public:
  explicit Board(const sf::Texture &texture) = delete;
  Board(const sf::Texture &texture, sf::Vector2f pos) = delete;
  explicit Board(sf::Vector2f pos);
  Board();

  // initializes the board by setting the texture of each square, define the size of the board and
  // giving the board itself an invisible texture
  void init(sf::Texture &texture_white, sf::Texture &texture_black, sf::Texture &texture_board);

  std::array<Entity, BOARD_SIZE * BOARD_SIZE> &getSquares();

  // These methods from Entity:: are redefined, because we now have to adjust every square also,
  // when drawing or reposition the board entity.
  void draw(sf::RenderWindow &window) override;
  void setPosition(const sf::Vector2f &pos) override;

 private:
  std::array<Entity, BOARD_SIZE * BOARD_SIZE> m_squares;
};
