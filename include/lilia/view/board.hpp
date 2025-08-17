#pragma once

// forward decleration
namespace sf {
class RenderWindow;
}

#include <array>

#include "../chess_types.hpp"
#include "entity.hpp"
#include "render_constants.hpp"

namespace lilia {

class Board : public Entity {
 public:
  Board(const sf::Texture &texture) = delete;
  Board(const sf::Texture &texture, Entity::Position pos) = delete;
  Board(Entity::Position pos);
  Board() = default;

  // initializes the board by setting the texture of each square, define the size of the board and
  // giving the board itself an invisible texture
  void init(const sf::Texture &texture_white, const sf::Texture &texture_black,
            const sf::Texture &texture_board);

  [[nodiscard]] Entity::Position getPosOfSquare(core::Square sq) const;

  // These methods from Entity:: are redefined, because we now have to adjust every square also,
  // when drawing or reposition the board entity.
  void draw(sf::RenderWindow &window) override;
  void setPosition(const Entity::Position &pos) override;

 private:
  std::array<Entity, core::BOARD_SIZE * core::BOARD_SIZE> m_squares;
};

}  // namespace lilia
