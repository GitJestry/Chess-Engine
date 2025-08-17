#pragma once
#include "../chess_types.hpp"
#include "entity.hpp"

namespace lilia {

class Piece : public Entity {
 public:
  Piece(core::PieceColor color, core::PieceType type, const sf::Texture &texture);
  Piece(core::PieceColor color, core::PieceType type, const sf::Texture &texture,
        Entity::Position pos);
  Piece() = default;

  void setColor(core::PieceColor color);
  [[nodiscard]] inline core::PieceColor getColor() const;
  void setType(core::PieceType type);
  [[nodiscard]] inline core::PieceType getType() const;

 private:
  core::PieceColor m_color;
  core::PieceType m_type;
};

}  // namespace lilia
