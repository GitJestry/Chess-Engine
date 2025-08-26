#include "lilia/view/game_view.hpp"

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <iostream>

namespace lilia::view {

GameView::GameView(sf::RenderWindow& window)
    : m_window(window),
      m_board_view(),
      m_piece_manager(m_board_view),
      m_highlight_manager(m_board_view),
      m_chess_animator(m_board_view, m_piece_manager) {
  m_cursor_default.loadFromSystem(sf::Cursor::Arrow);

  sf::Image openImg;
  if (openImg.loadFromFile(constant::STR_FILE_PATH_HAND_OPEN)) {
    m_cursor_hand_open.loadFromPixels(openImg.getPixelsPtr(), openImg.getSize(),
                                      {openImg.getSize().x / 2, openImg.getSize().y / 2});
  }
  sf::Image closedImg;
  if (closedImg.loadFromFile(constant::STR_FILE_PATH_HAND_CLOSED)) {
    m_cursor_hand_closed.loadFromPixels(closedImg.getPixelsPtr(), closedImg.getSize(),
                                        {openImg.getSize().x / 2, openImg.getSize().y / 2});
  }
  m_window.setMouseCursor(m_cursor_default);
}

void GameView::init(const std::string& fen) {
  m_board_view.init();
  m_piece_manager.initFromFen(fen);
}

void GameView::update(float dt) {
  m_chess_animator.updateAnimations(dt);
}

void GameView::render() {
  m_board_view.renderBoard(m_window);
  m_highlight_manager.renderSelect(m_window);
  m_chess_animator.renderHighlightLevel(m_window);
  m_highlight_manager.renderHover(m_window);
  m_piece_manager.renderPieces(m_window, m_chess_animator);
  m_highlight_manager.renderAttack(m_window);
  m_chess_animator.render(m_window);
}

void GameView::resetBoard() {
  m_piece_manager.removeAll();
  init();
}

void GameView::warningKingSquareAnim(core::Square ksq) {
  m_chess_animator.warningAnim(ksq);
  m_chess_animator.declareHighlightLevel(ksq);
}

void GameView::animationSnapAndReturn(core::Square sq, core::MousePos mousePos) {
  m_chess_animator.snapAndReturn(sq, mousePos);
}

void GameView::animationMovePiece(core::Square from, core::Square to, core::Square enPSquare,
                                  core::PieceType promotion) {
  m_chess_animator.movePiece(from, to, promotion);
  if (enPSquare != core::NO_SQUARE) m_piece_manager.removePiece(enPSquare);
}

void GameView::animationDropPiece(core::Square from, core::Square to, core::Square enPSquare,
                                  core::PieceType promotion) {
  m_chess_animator.dropPiece(from, to, promotion);
  if (enPSquare != core::NO_SQUARE) m_piece_manager.removePiece(enPSquare);
}

void GameView::playPiecePlaceHolderAnimation(core::Square sq) {
  m_chess_animator.piecePlaceHolder(sq);
}

void GameView::playPromotionSelectAnim(core::Square promSq, core::Color c) {
  m_chess_animator.promotionSelect(promSq, m_promotion_manager, c);
}

void GameView::endAnimation(core::Square sq) {
  m_chess_animator.end(sq);
}

[[nodiscard]] bool GameView::hasPieceOnSquare(core::Square pos) const {
  return m_piece_manager.hasPieceOnSquare(pos);
}

[[nodiscard]] bool GameView::isSameColorPiece(core::Square sq1, core::Square sq2) const {
  return m_piece_manager.isSameColor(sq1, sq2);
}

void GameView::highlightSquare(core::Square pos) {
  m_highlight_manager.highlightSquare(pos);
}
void GameView::highlightHoverSquare(core::Square pos) {
  m_highlight_manager.highlightHoverSquare(pos);
}
void GameView::highlightAttackSquare(core::Square pos) {
  m_highlight_manager.highlightAttackSquare(pos);
}
void GameView::highlightCaptureSquare(core::Square pos) {
  m_highlight_manager.highlightCaptureSquare(pos);
}

void GameView::clearHighlightSquare(core::Square pos) {
  m_highlight_manager.clearHighlightSquare(pos);
}

void GameView::clearHighlightHoverSquare(core::Square pos) {
  m_highlight_manager.clearHighlightHoverSquare(pos);
}

void GameView::clearAllHighlights() {
  m_highlight_manager.clearAllHighlights();
}

bool GameView::isInPromotionSelection() {
  return m_promotion_manager.hasOptions();
}

core::PieceType GameView::getSelectedPromotion(core::MousePos mousePos) {
  return m_promotion_manager.clickedOnType(static_cast<Entity::Position>(mousePos));
}

void GameView::removePromotionSelection() {
  m_promotion_manager.removeOptions();
}

void GameView::showGameOver(core::GameResult res, core::Color sideToMove) {
  std::cout << "Game is Over!" << std::endl;
  switch (res) {
    case core::GameResult::CHECKMATE:
      std::cout << "CHECKMATE -> "
                << (sideToMove == core::Color::White ? "Black won" : "White won");
      break;
    case core::GameResult::REPETITION:
      std::cout << "REPITITION -> Draw!";
      break;
    case core::GameResult::MOVERULE:
      std::cout << "MOVERULE-> Draw!";
      break;
    case core::GameResult::STALEMATE:
      std::cout << "STALEMATE -> Draw!";
      break;

    default:
      std::cout << "result is not correct";
  }
  std::cout << std::endl;
}

[[nodiscard]] core::Square GameView::mousePosToSquare(core::MousePos mousePos) const {
  int file = mousePos.x / constant::SQUARE_PX_SIZE;
  int rankSFML = mousePos.y / constant::SQUARE_PX_SIZE;

  int rankFromWhite = 7 - rankSFML;

  if (file < 0) file = 0;
  if (file > 7) file = 7;
  if (rankFromWhite < 0) rankFromWhite = 0;
  if (rankFromWhite > 7) rankFromWhite = 7;

  return static_cast<core::Square>(rankFromWhite * 8 + file);
}

void GameView::setPieceToMouseScreenPos(core::Square pos, core::MousePos mousePos) {
  m_piece_manager.setPieceToScreenPos(pos, mousePos);
}
void GameView::setPieceToSquareScreenPos(core::Square from, core::Square to) {
  m_piece_manager.setPieceToSquareScreenPos(from, to);
}
void GameView::setDefaultCursor() {
  m_window.setMouseCursor(m_cursor_default);
}
void GameView::setHandOpenCursor() {
  m_window.setMouseCursor(m_cursor_hand_open);
}
void GameView::setHandClosedCursor() {
  m_window.setMouseCursor(m_cursor_hand_closed);
}

sf::Vector2u GameView::getWindowSize() const {
  return m_window.getSize();
}

Entity::Position GameView::getPieceSize(core::Square pos) const {
  return m_piece_manager.getPieceSize(pos);
}

}  // namespace lilia::view
