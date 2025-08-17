#include "lilia/view/board_view.hpp"

#include "lilia/view/texture_table.hpp"

namespace lilia {

BoardView::BoardView() : m_board({core::WINDOW_PX_SIZE / 2, core::WINDOW_PX_SIZE / 2}) {}

void BoardView::init() {
  m_board.init(TextureTable::getInstance().get(core::STR_TEXTURE_WHITE),
               TextureTable::getInstance().get(core::STR_TEXTURE_BLACK),
               TextureTable::getInstance().get(core::STR_TEXTURE_TRANSPARENT));
}

void BoardView::renderBoard(sf::RenderWindow& window) {
  m_board.draw(window);
}
[[nodiscard]] Entity::Position BoardView::getSquareScreenPos(core::Square sq) const {
  return m_board.getPosOfSquare(sq);
}

}  // namespace lilia
