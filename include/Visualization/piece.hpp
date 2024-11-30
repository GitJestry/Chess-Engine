#pragma once
#include <entity.hpp>
#include <piece_color.hpp>
#include <piece_type.hpp>

class Piece : Entity {

  public:
    
    Piece(PieceColor color, PieceType type, const sf::Texture& texture);
    Piece(PieceColor color, PieceType type, const sf::Texture& texture, sf::Vector2f pos);

    void SetColor(PieceColor color);
    PieceColor GetColor();
    void SetType(PieceType type);
    PieceType GetType();

  private:
    PieceColor color_;
    PieceType type_;

};