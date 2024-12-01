#pragma once
#include <entity.hpp>
#include <array>
#include <SFML/Window/Window.hpp>

class Board : public Entity
{

public:
  explicit Board(const sf::Texture &texture) = delete;
  Board(const sf::Texture &texture, sf::Vector2f pos) = delete;
  explicit Board(sf::Vector2f pos);
  Board();

  // initializes the board by setting the texture of each square, define the size of the board and giving the board itself an invisible texture
  void initialize(sf::Texture &texture_white, sf::Texture &texture_black, sf::Texture &texture_board, float board_size);

  std::array<Entity, 64> &getSquares();

  // These methods from Entity:: are redefined, because we now have to adjust every square also,
  // whe drawing or reposition the board entity.
  void draw(sf::RenderWindow &window) override;
  void setPosition(const sf::Vector2f &pos) override;

private:
  std::array<Entity, 64> squares_;
};
