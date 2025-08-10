#include "visuals/piece.hpp"

Piece::Piece(PieceColor color, PieceType type, const sf::Texture &texture)
    : m_color(color), m_type(type), Entity(texture) {}
Piece::Piece(PieceColor color, PieceType type, const sf::Texture &texture, sf::Vector2f pos)
    : m_color(color), m_type(type), Entity(texture, pos) {}

void Piece::setColor(PieceColor color) {
  m_color = color;
}
PieceColor Piece::getColor() const {
  return m_color;
}
void Piece::setType(PieceType type) {
  m_type = type;
}
PieceType Piece::getType() const {
  return m_type;
}
