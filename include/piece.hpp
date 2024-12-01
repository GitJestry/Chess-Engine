#pragma once
#include <entity.hpp>
#include <piece_color_enum.hpp>
#include <piece_type_enum.hpp>

class Piece : public Entity
{

public:
  Piece(PieceColor color, PieceType type, const sf::Texture &texture);
  Piece(PieceColor color, PieceType type, const sf::Texture &texture, sf::Vector2f pos);

  void setColor(PieceColor color);
  PieceColor getColor() const;
  void setType(PieceType type);
  PieceType getType() const;

private:
  PieceColor color_;
  PieceType type_;
};
