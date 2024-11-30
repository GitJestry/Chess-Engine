#include <piece.hpp>

Piece::Piece(PieceColor color, PieceType type, const sf::Texture& texture) : color_(color), type_(type), Entity(texture) {} 
Piece::Piece(PieceColor color, PieceType type, const sf::Texture& texture, sf::Vector2f pos) 
: color_(color), type_(type), Entity(texture, pos) {}

void Piece::SetColor(PieceColor color) { this->color_ = color; }
PieceColor Piece::GetColor() { return this->color_; }
void Piece::SetType(PieceType type) {  this->type_ = type; }
PieceType Piece::GetType() { return this->type_; }
