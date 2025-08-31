#include "lilia/view/game_view.hpp"

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <algorithm>
#include <iostream>
#include <limits>

#include "lilia/bot/bot_info.hpp"
#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view {

GameView::GameView(sf::RenderWindow &window, bool topIsBot, bool bottomIsBot)
    : m_window(window),
      m_board_view(),
      m_piece_manager(m_board_view),
      m_highlight_manager(m_board_view),
      m_chess_animator(m_board_view, m_piece_manager),
      m_eval_bar(),
      m_move_list(),
      m_top_player(),
      m_bottom_player() {
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

  PlayerInfo topInfo;
  if (topIsBot) {
    topInfo = getBotInfo(BotType::Lilia);
  } else {
    topInfo = {"Challenger", 0, constant::STR_FILE_PATH_ICON_CHALLENGER};
    m_board_view.setFlipped(true);
  }
  m_top_player.setInfo(topInfo);

  PlayerInfo bottomInfo;
  if (bottomIsBot) {
    bottomInfo = getBotInfo(BotType::Lilia);
  } else {
    bottomInfo = {"Challenger", 0, constant::STR_FILE_PATH_ICON_CHALLENGER};
    m_board_view.setFlipped(false);
  }
  m_bottom_player.setInfo(bottomInfo);

  layout(m_window.getSize().x, m_window.getSize().y);
}

void GameView::init(const std::string &fen) {
  m_board_view.init();
  m_piece_manager.initFromFen(fen);
}

void GameView::update(float dt) {
  m_chess_animator.updateAnimations(dt);
}

void GameView::updateEval(int eval) {
  m_eval_bar.update(eval);
}

void GameView::render() {
  m_eval_bar.render(m_window);
  m_board_view.renderBoard(m_window);
  m_top_player.render(m_window);
  m_bottom_player.render(m_window);
  m_highlight_manager.renderSelect(m_window);
  m_chess_animator.renderHighlightLevel(m_window);
  m_highlight_manager.renderHover(m_window);
  m_piece_manager.renderPieces(m_window, m_chess_animator);
  m_highlight_manager.renderAttack(m_window);
  m_chess_animator.render(m_window);
  m_move_list.render(m_window);
}

void GameView::addMove(const std::string &move) {
  m_move_list.addMove(move);
}

void GameView::selectMove(std::size_t moveIndex) {
  m_move_list.setCurrentMove(moveIndex);
}

void GameView::setBoardFen(const std::string &fen) {
  m_chess_animator.cancelAll();
  m_piece_manager.removeAll();
  m_piece_manager.initFromFen(fen);
  m_highlight_manager.clearAllHighlights();
}

void GameView::scrollMoveList(float delta) {
  m_move_list.scroll(delta);
}

void GameView::setBotMode(bool anyBot) {
  m_move_list.setBotMode(anyBot);
}

std::size_t GameView::getMoveIndexAt(core::MousePos mousePos) const {
  return m_move_list.getMoveIndexAt(
      Entity::Position{static_cast<float>(mousePos.x), static_cast<float>(mousePos.y)});
}

void GameView::layout(unsigned int width, unsigned int height) {
  float vMargin = std::max(
      0.f, (static_cast<float>(height) - static_cast<float>(constant::WINDOW_PX_SIZE)) / 2.f);
  float hMargin = std::max(
      0.f, (static_cast<float>(width) - static_cast<float>(constant::WINDOW_TOTAL_WIDTH)) / 2.f);

  float boardCenterX = hMargin +
                       static_cast<float>(constant::EVAL_BAR_WIDTH + constant::SIDE_MARGIN) +
                       static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f;
  float boardCenterY = vMargin + static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f;

  m_board_view.setPosition({boardCenterX, boardCenterY});

  float evalCenterX =
      hMargin + static_cast<float>(constant::EVAL_BAR_WIDTH + constant::SIDE_MARGIN) / 2.f;
  m_eval_bar.setPosition({evalCenterX, boardCenterY});

  float moveListX = hMargin + static_cast<float>(constant::EVAL_BAR_WIDTH + constant::SIDE_MARGIN +
                                                 constant::WINDOW_PX_SIZE + constant::SIDE_MARGIN);
  m_move_list.setPosition({moveListX, vMargin});
  m_move_list.setSize(constant::MOVE_LIST_WIDTH, constant::WINDOW_PX_SIZE);

  float boardLeft = boardCenterX - static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f;
  float boardTop = boardCenterY - static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f;
  // shift player info slightly to the right and adjust vertical placement
  m_top_player.setPositionClamped({boardLeft + 5.f, boardTop - 45.f}, m_window.getSize());
  m_bottom_player.setPositionClamped(
      {boardLeft + 5.f, boardTop + static_cast<float>(constant::WINDOW_PX_SIZE) + 15.f},
      m_window.getSize());
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

[[nodiscard]] core::PieceType GameView::getPieceType(core::Square pos) const {
  return m_piece_manager.getPieceType(pos);
}

[[nodiscard]] core::Color GameView::getPieceColor(core::Square pos) const {
  return m_piece_manager.getPieceColor(pos);
}

void GameView::addPiece(core::PieceType type, core::Color color, core::Square pos) {
  m_piece_manager.addPiece(type, color, pos);
}

void GameView::removePiece(core::Square pos) { m_piece_manager.removePiece(pos); }

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

static inline int normalizeUnsignedToSigned(unsigned int u) {
  // Mappe 0..INT_MAX -> 0..INT_MAX, und (INT_MAX+1 .. UINT_MAX) -> negative
  // Werte
  if (u <= static_cast<unsigned int>(std::numeric_limits<int>::max())) return static_cast<int>(u);
  // -(UINT_MAX - u + 1)  (zweierkomplement-konsistent)
  return -static_cast<int>((std::numeric_limits<unsigned int>::max() - u) + 1u);
}
// clamp eines int auf inklusiven Bereich [lo, hi], hi >= lo
constexpr int clampInt(int v, int lo, int hi) noexcept {
  return (v < lo) ? lo : (v > hi ? hi : v);
}

core::MousePos GameView::clampPosToBoard(core::MousePos mousePos) const noexcept {
  // 1) Unsigned -> Signed normalisieren (gegen negative Mauswerte außerhalb des
  // Fensters)
  const int sx = normalizeUnsignedToSigned(mousePos.x);
  const int sy = normalizeUnsignedToSigned(mousePos.y);

  // 2) Brettgrenzen bestimmen
  auto boardCenter = m_board_view.getPosition();
  const int left =
      static_cast<int>(boardCenter.x - static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f);
  const int top =
      static_cast<int>(boardCenter.y - static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f);
  const int right = left + static_cast<int>(constant::WINDOW_PX_SIZE) - 1;
  const int bottom = top + static_cast<int>(constant::WINDOW_PX_SIZE) - 1;

  // 3) Clamp innerhalb der Brettgrenzen
  const int cx = clampInt(sx, left, right);
  const int cy = clampInt(sy, top, bottom);

  // 4) Nach Clamp garantiert innerhalb -> sicher auf unsigned
  return {static_cast<unsigned>(cx), static_cast<unsigned>(cy)};
}

[[nodiscard]] core::Square GameView::mousePosToSquare(core::MousePos mousePos) const {
  auto boardCenter = m_board_view.getPosition();
  float originX = boardCenter.x - static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f;
  float originY = boardCenter.y - static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f;
  float right = originX + static_cast<float>(constant::WINDOW_PX_SIZE);
  float bottom = originY + static_cast<float>(constant::WINDOW_PX_SIZE);

  if (mousePos.x < originX || mousePos.x >= right || mousePos.y < originY || mousePos.y >= bottom) {
    return core::NO_SQUARE;
  }

  int fileSFML = static_cast<int>((mousePos.x - originX) / constant::SQUARE_PX_SIZE);
  int rankSFML = static_cast<int>((mousePos.y - originY) / constant::SQUARE_PX_SIZE);

  int fileFromWhite;
  int rankFromWhite;
  if (m_board_view.isFlipped()) {
    fileFromWhite = 7 - fileSFML;
    rankFromWhite = rankSFML;
  } else {
    fileFromWhite = fileSFML;
    rankFromWhite = 7 - rankSFML;
  }

  return static_cast<core::Square>(rankFromWhite * 8 + fileFromWhite);
}

void GameView::setPieceToMouseScreenPos(core::Square pos, core::MousePos mousePos) {
  m_piece_manager.setPieceToScreenPos(pos, clampPosToBoard(mousePos));
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

void GameView::toggleBoardOrientation() {
  m_board_view.toggleFlipped();
}

[[nodiscard]] bool GameView::isOnFlipIcon(core::MousePos mousePos) const {
  return m_board_view.isOnFlipIcon(mousePos);
}

}  // namespace lilia::view
