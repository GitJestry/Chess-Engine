#include "lilia/view/game_view.hpp"

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>
#include <algorithm>

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
                                      {openImg.getSize().x / 3, openImg.getSize().y / 3});
  }
  sf::Image closedImg;
  if (closedImg.loadFromFile(constant::STR_FILE_PATH_HAND_CLOSED)) {
    m_cursor_hand_closed.loadFromPixels(closedImg.getPixelsPtr(), closedImg.getSize(),
                                        {openImg.getSize().x / 3, openImg.getSize().y / 3});
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

  m_font.loadFromFile(constant::STR_FILE_PATH_FONT);
  m_popup_bg.setFillColor(sf::Color(40, 40, 40, 220));
  m_popup_bg.setSize({300.f, 150.f});
  m_popup_bg.setOrigin(m_popup_bg.getSize().x / 2.f, m_popup_bg.getSize().y / 2.f);
  m_popup_msg.setFont(m_font);
  m_popup_msg.setCharacterSize(20);
  m_popup_msg.setFillColor(sf::Color::White);
  m_popup_yes.setFont(m_font);
  m_popup_yes.setCharacterSize(18);
  m_popup_yes.setFillColor(sf::Color::White);
  m_popup_no.setFont(m_font);
  m_popup_no.setCharacterSize(18);
  m_popup_no.setFillColor(sf::Color::White);
  m_go_msg.setFont(m_font);
  m_go_msg.setCharacterSize(20);
  m_go_msg.setFillColor(sf::Color::White);
  m_go_new_bot.setFont(m_font);
  m_go_new_bot.setCharacterSize(18);
  m_go_new_bot.setFillColor(sf::Color::White);
  m_go_rematch.setFont(m_font);
  m_go_rematch.setCharacterSize(18);
  m_go_rematch.setFillColor(sf::Color::White);
}

void GameView::init(const std::string &fen) {
  m_board_view.init();
  m_piece_manager.initFromFen(fen);
  m_eval_bar.setResult("");
  m_eval_bar.update(0);
}

void GameView::update(float dt) {
  m_chess_animator.updateAnimations(dt);
}

void GameView::updateEval(int eval) {
  m_eval_bar.update(eval);
}

void GameView::setEvalResult(const std::string &result) {
  m_eval_bar.setResult(result);
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

  if (m_show_resign || m_show_game_over) {
    sf::RectangleShape overlay(
        {static_cast<float>(m_window.getSize().x), static_cast<float>(m_window.getSize().y)});
    overlay.setFillColor(sf::Color(0, 0, 0, 100));
    m_window.draw(overlay);
    m_window.draw(m_popup_bg);
    if (m_show_resign) {
      m_window.draw(m_popup_msg);
      m_window.draw(m_popup_yes);
      m_window.draw(m_popup_no);
    } else if (m_show_game_over) {
      m_window.draw(m_go_msg);
      m_window.draw(m_go_new_bot);
      m_window.draw(m_go_rematch);
    }
  }
}

void GameView::addMove(const std::string &move) {
  m_move_list.addMove(move);
}

void GameView::addResult(const std::string &result) {
  m_move_list.addResult(result);
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

MoveListView::Option GameView::getOptionAt(core::MousePos mousePos) const {
  return m_move_list.getOptionAt(
      Entity::Position{static_cast<float>(mousePos.x), static_cast<float>(mousePos.y)});
}

void GameView::setGameOver(bool over) {
  m_move_list.setGameOver(over);
}

void GameView::showResignPopup() {
  m_show_resign = true;
  m_popup_bg.setPosition(static_cast<float>(m_window.getSize().x) / 2.f,
                         static_cast<float>(m_window.getSize().y) / 2.f);
  m_popup_msg.setString("Do you really want to resign?");
  auto b = m_popup_msg.getLocalBounds();
  m_popup_msg.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
  m_popup_msg.setPosition(m_popup_bg.getPosition().x, m_popup_bg.getPosition().y - 20.f);
  m_popup_yes.setString("Yes");
  m_popup_no.setString("No");
  auto yb = m_popup_yes.getLocalBounds();
  auto nb = m_popup_no.getLocalBounds();
  m_popup_yes.setOrigin(yb.left + yb.width / 2.f, yb.top + yb.height / 2.f);
  m_popup_no.setOrigin(nb.left + nb.width / 2.f, nb.top + nb.height / 2.f);
  float cx = m_popup_bg.getPosition().x;
  float cy = m_popup_bg.getPosition().y + 30.f;
  m_popup_yes.setPosition(cx - 40.f, cy);
  m_popup_no.setPosition(cx + 40.f, cy);
  m_yes_bounds = m_popup_yes.getGlobalBounds();
  m_no_bounds = m_popup_no.getGlobalBounds();
}

void GameView::hideResignPopup() {
  m_show_resign = false;
}

bool GameView::isResignPopupOpen() const {
  return m_show_resign;
}

bool GameView::isOnResignYes(core::MousePos mousePos) const {
  return m_yes_bounds.contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
}

bool GameView::isOnResignNo(core::MousePos mousePos) const {
  return m_no_bounds.contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
}

void GameView::showGameOverPopup(const std::string &msg) {
  m_show_game_over = true;
  m_popup_bg.setPosition(static_cast<float>(m_window.getSize().x) / 2.f,
                         static_cast<float>(m_window.getSize().y) / 2.f);
  m_go_msg.setString(msg);
  auto b = m_go_msg.getLocalBounds();
  m_go_msg.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
  m_go_msg.setPosition(m_popup_bg.getPosition().x, m_popup_bg.getPosition().y - 20.f);
  m_go_new_bot.setString("New Bot");
  m_go_rematch.setString("Rematch");
  auto nb = m_go_new_bot.getLocalBounds();
  auto rb = m_go_rematch.getLocalBounds();
  m_go_new_bot.setOrigin(nb.left + nb.width / 2.f, nb.top + nb.height / 2.f);
  m_go_rematch.setOrigin(rb.left + rb.width / 2.f, rb.top + rb.height / 2.f);
  float cx = m_popup_bg.getPosition().x;
  float cy = m_popup_bg.getPosition().y + 30.f;
  m_go_new_bot.setPosition(cx - 60.f, cy);
  m_go_rematch.setPosition(cx + 60.f, cy);
  m_nb_bounds = m_go_new_bot.getGlobalBounds();
  m_rm_bounds = m_go_rematch.getGlobalBounds();
}

void GameView::hideGameOverPopup() {
  m_show_game_over = false;
}

bool GameView::isGameOverPopupOpen() const {
  return m_show_game_over;
}

bool GameView::isOnNewBot(core::MousePos mousePos) const {
  return m_nb_bounds.contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
}

bool GameView::isOnRematch(core::MousePos mousePos) const {
  return m_rm_bounds.contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
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
                                  core::PieceType promotion, std::function<void()> onComplete) {
  m_chess_animator.movePiece(from, to, promotion, std::move(onComplete));
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

void GameView::removePiece(core::Square pos) {
  m_piece_manager.removePiece(pos);
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

[[nodiscard]] core::Square GameView::mousePosToSquare(core::MousePos mousePos) const {
  return m_board_view.mousePosToSquare(mousePos);
}

void GameView::setPieceToMouseScreenPos(core::Square pos, core::MousePos mousePos) {
  m_piece_manager.setPieceToScreenPos(pos, m_board_view.clampPosToBoard(mousePos));
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
