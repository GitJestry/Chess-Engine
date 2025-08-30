#include "lilia/view/board_view.hpp"

#include "lilia/view/texture_table.hpp"

namespace lilia::view {

BoardView::BoardView()
    : m_board({static_cast<float>(constant::BOARD_OFFSET_X) +
                constant::WINDOW_PX_SIZE / 2.0f,
               static_cast<float>(constant::BOARD_OFFSET_Y) +
                constant::WINDOW_PX_SIZE / 2.0f}) {}

void BoardView::init() {
  m_board.init(TextureTable::getInstance().get(constant::STR_TEXTURE_WHITE),
               TextureTable::getInstance().get(constant::STR_TEXTURE_BLACK),
               TextureTable::getInstance().get(constant::STR_TEXTURE_TRANSPARENT));
}

void BoardView::resize(const sf::Vector2u& windowSize) {
  (void)windowSize;
  m_board.setPosition({static_cast<float>(constant::BOARD_OFFSET_X) +
                           constant::WINDOW_PX_SIZE / 2.0f,
                       static_cast<float>(constant::BOARD_OFFSET_Y) +
                           constant::WINDOW_PX_SIZE / 2.0f});
  init();
}

void BoardView::renderBoard(sf::RenderWindow& window) {
  m_board.draw(window);
}
[[nodiscard]] Entity::Position BoardView::getSquareScreenPos(core::Square sq) const {
  return m_board.getPosOfSquare(sq);
}

}  // namespace lilia::view
