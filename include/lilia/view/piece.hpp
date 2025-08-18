#pragma once
#include "entity.hpp"
#include "render_types.hpp"

namespace lilia {

class Piece : public Entity {
 public:
  Piece(core::PieceColor color, core::PieceType type, const sf::Texture &texture);
  Piece(core::PieceColor color, core::PieceType type, const sf::Texture &texture,
        Entity::Position pos);
  Piece() = default;

  void setColor(core::PieceColor color);
  core::PieceColor getColor() const;
  void setType(core::PieceType type);
  core::PieceType getType() const;

 private:
  core::PieceColor m_color;
  core::PieceType m_type;
};

}  // namespace lilia
