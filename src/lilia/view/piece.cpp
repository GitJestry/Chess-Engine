#include "lilia/view/piece.hpp"

namespace lilia {

Piece::Piece(core::PieceColor color, core::PieceType type, const sf::Texture &texture)
    : m_color(color), m_type(type), Entity(texture) {}
Piece::Piece(core::PieceColor color, core::PieceType type, const sf::Texture &texture,
             Entity::Position pos)
    : m_color(color), m_type(type), Entity(texture, pos) {}

void Piece::setColor(core::PieceColor color) {
  m_color = color;
}
core::PieceColor Piece::getColor() const {
  return m_color;
}
void Piece::setType(core::PieceType type) {
  m_type = type;
}
core::PieceType Piece::getType() const {
  return m_type;
}

}  // namespace lilia
