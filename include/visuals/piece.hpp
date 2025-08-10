#pragma once
#include <types.hpp>

#include "./entity.hpp"

class Piece : public Entity {
 public:
  Piece(PieceColor color, PieceType type, const sf::Texture &texture);
  Piece(PieceColor color, PieceType type, const sf::Texture &texture, sf::Vector2f pos);
  Piece() = default;

  void setColor(PieceColor color);
  PieceColor getColor() const;
  void setType(PieceType type);
  PieceType getType() const;

 private:
  PieceColor m_color;
  PieceType m_type;
};
