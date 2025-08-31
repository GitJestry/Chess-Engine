#include "lilia/view/board_view.hpp"

#include "lilia/view/texture_table.hpp"

namespace lilia::view {

BoardView::BoardView()
    : m_board({constant::WINDOW_PX_SIZE / 2, constant::WINDOW_PX_SIZE / 2}),
      m_flipped(false) {}

void BoardView::init() {
  m_board.init(TextureTable::getInstance().get(constant::STR_TEXTURE_WHITE),
               TextureTable::getInstance().get(constant::STR_TEXTURE_BLACK),
               TextureTable::getInstance().get(constant::STR_TEXTURE_TRANSPARENT));
  setPosition(getPosition());
}

void BoardView::renderBoard(sf::RenderWindow& window) {
  m_board.draw(window);
}
[[nodiscard]] Entity::Position BoardView::getSquareScreenPos(core::Square sq) const {
  if (m_flipped) {
    return m_board.getPosOfSquare(
        static_cast<core::Square>(constant::BOARD_SIZE * constant::BOARD_SIZE - 1 - sq));
  }
  return m_board.getPosOfSquare(sq);
}

void BoardView::toggleFlipped() {
  m_flipped = !m_flipped;
  m_board.setFlipped(m_flipped);
}

void BoardView::setFlipped(bool flipped) {
  m_flipped = flipped;
  m_board.setFlipped(m_flipped);
}

[[nodiscard]] bool BoardView::isFlipped() const {
  return m_flipped;
}

void BoardView::setPosition(const Entity::Position& pos) {
  m_board.setPosition(pos);
}

[[nodiscard]] Entity::Position BoardView::getPosition() const {
  return m_board.getPosition();
}

}  // namespace lilia::view
