#pragma once

namespace sf {
class RenderWindow;
class Texture;
}

#include <array>
#include <SFML/Graphics/Texture.hpp>

#include "../chess_types.hpp"
#include "entity.hpp"
#include "render_constants.hpp"

namespace lilia::view {

class Board : public Entity {
 public:
  Board(const sf::Texture &texture) = delete;
  Board(const sf::Texture &texture, Entity::Position pos) = delete;
  Board(Entity::Position pos);
  Board() = default;

  void init(const sf::Texture &texture_white, const sf::Texture &texture_black,
            const sf::Texture &texture_board);

  [[nodiscard]] Entity::Position getPosOfSquare(core::Square sq) const;

  void draw(sf::RenderWindow &window) override;
  void setPosition(const Entity::Position &pos) override;

  void setFlipped(bool flipped);
  [[nodiscard]] bool isFlipped() const;

 private:
  void positionLabels(Entity::Position offset);
  std::array<Entity, constant::BOARD_SIZE * constant::BOARD_SIZE> m_squares;
  std::array<Entity, constant::BOARD_SIZE> m_file_labels;
  std::array<Entity, constant::BOARD_SIZE> m_rank_labels;
  std::array<sf::Texture, constant::BOARD_SIZE> m_file_textures;
  std::array<sf::Texture, constant::BOARD_SIZE> m_rank_textures;
  bool m_flipped{false};
};

}  // namespace lilia::view
