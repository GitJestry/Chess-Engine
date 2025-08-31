#include "lilia/view/highlight_manager.hpp"

#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view {

HighlightManager::HighlightManager(const BoardView& boardRef)
    : m_board_view_ref(boardRef),
      m_hl_attack_squares(),
      m_hl_select_squares(),
      m_hl_hover_squares(),
      m_hl_premove_squares() {}

void HighlightManager::renderEntitiesToBoard(std::unordered_map<core::Square, Entity>& map,
                                             sf::RenderWindow& window) {
  for (auto& pair : map) {
    const auto& pos = pair.first;
    auto& entity = pair.second;
    entity.setPosition(m_board_view_ref.getSquareScreenPos(pos));
    entity.draw(window);
  }
}

void HighlightManager::renderAttack(sf::RenderWindow& window) {
  renderEntitiesToBoard(m_hl_attack_squares, window);
}
void HighlightManager::renderHover(sf::RenderWindow& window) {
  renderEntitiesToBoard(m_hl_hover_squares, window);
}
void HighlightManager::renderSelect(sf::RenderWindow& window) {
  renderEntitiesToBoard(m_hl_select_squares, window);
}
void HighlightManager::renderPremove(sf::RenderWindow& window) {
  renderEntitiesToBoard(m_hl_premove_squares, window);
}

void HighlightManager::highlightSquare(core::Square pos) {
  Entity newSelectHlight(TextureTable::getInstance().get(constant::STR_TEXTURE_SELECTHLIGHT));
  newSelectHlight.setScale(constant::SQUARE_PX_SIZE, constant::SQUARE_PX_SIZE);
  m_hl_select_squares[pos] = std::move(newSelectHlight);
}
void HighlightManager::highlightAttackSquare(core::Square pos) {
  Entity newAttackHlight(TextureTable::getInstance().get(constant::STR_TEXTURE_ATTACKHLIGHT));
  m_hl_attack_squares[pos] = std::move(newAttackHlight);
}
void HighlightManager::highlightCaptureSquare(core::Square pos) {
  Entity newCaptureHlight(TextureTable::getInstance().get(constant::STR_TEXTURE_CAPTUREHLIGHT));
  m_hl_attack_squares[pos] = std::move(newCaptureHlight);
}

void HighlightManager::highlightHoverSquare(core::Square pos) {
  Entity newHoverHlight(TextureTable::getInstance().get(constant::STR_TEXTURE_HOVERHLIGHT));
  m_hl_hover_squares[pos] = std::move(newHoverHlight);
}
void HighlightManager::highlightPremoveSquare(core::Square pos) {
  Entity newPreHlight(TextureTable::getInstance().get(constant::STR_TEXTURE_PREMOVEHLIGHT));
  newPreHlight.setScale(constant::SQUARE_PX_SIZE, constant::SQUARE_PX_SIZE);
  m_hl_premove_squares[pos] = std::move(newPreHlight);
}
void HighlightManager::clearAllHighlights() {
  m_hl_select_squares.clear();
  m_hl_attack_squares.clear();
  m_hl_hover_squares.clear();
  m_hl_premove_squares.clear();
}
void HighlightManager::clearHighlightSquare(core::Square pos) {
  m_hl_select_squares.erase(pos);
}
void HighlightManager::clearHighlightHoverSquare(core::Square pos) {
  m_hl_hover_squares.erase(pos);
}
void HighlightManager::clearPremoveHighlights() { m_hl_premove_squares.clear(); }

}  // namespace lilia::view
