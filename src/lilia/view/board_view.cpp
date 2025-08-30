#include "lilia/view/board_view.hpp"

#include "lilia/view/texture_table.hpp"

namespace lilia::view {

BoardView::BoardView()
    : m_board({constant::WINDOW_PX_SIZE / 2, constant::WINDOW_PX_SIZE / 2}),
      m_flip_icon(),
      m_flipped(false) {}

void BoardView::init() {
  m_board.init(TextureTable::getInstance().get(constant::STR_TEXTURE_WHITE),
               TextureTable::getInstance().get(constant::STR_TEXTURE_BLACK),
               TextureTable::getInstance().get(constant::STR_TEXTURE_TRANSPARENT));
  m_flip_icon.setTexture(
      TextureTable::getInstance().get("assets/textures/flip.png"));
  auto size = m_flip_icon.getOriginalSize();
  float scale = (constant::SQUARE_PX_SIZE * 0.5f) / size.x;
  m_flip_icon.setScale(scale, scale);
  m_flip_icon.setOriginToCenter();
  setPosition(getPosition());
}

void BoardView::renderBoard(sf::RenderWindow& window) {
  m_board.draw(window);
  m_flip_icon.draw(window);
}
[[nodiscard]] Entity::Position BoardView::getSquareScreenPos(core::Square sq) const {
  if (m_flipped) {
    return m_board.getPosOfSquare(static_cast<core::Square>(
        constant::BOARD_SIZE * constant::BOARD_SIZE - 1 - sq));
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

[[nodiscard]] bool BoardView::isFlipped() const { return m_flipped; }

void BoardView::setPosition(const Entity::Position& pos) {
  m_board.setPosition(pos);
  float iconOffset = constant::SQUARE_PX_SIZE * 0.5f;
  m_flip_icon.setPosition({pos.x + constant::WINDOW_PX_SIZE / 2.f - iconOffset,
                           pos.y + constant::WINDOW_PX_SIZE / 2.f + iconOffset});
}

[[nodiscard]] Entity::Position BoardView::getPosition() const {
  return m_board.getPosition();
}

[[nodiscard]] bool BoardView::isOnFlipIcon(core::MousePos mousePos) const {
  auto pos = m_flip_icon.getPosition();
  auto size = m_flip_icon.getCurrentSize();
  float left = pos.x - size.x / 2.f;
  float right = pos.x + size.x / 2.f;
  float top = pos.y - size.y / 2.f;
  float bottom = pos.y + size.y / 2.f;
  return mousePos.x >= left && mousePos.x <= right && mousePos.y >= top &&
         mousePos.y <= bottom;
}

}  // namespace lilia::view
