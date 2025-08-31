#include "lilia/view/game_view.hpp"

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>
#include <algorithm>

#include "lilia/bot/bot_info.hpp"
#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace {
const sf::Color colHeaderBG(42, 48, 63);
const sf::Color colHoverBG(58, 66, 84);
const sf::Color colBorder(120, 140, 170, 50);
const sf::Color colText(240, 244, 255);
const sf::Color colAccentHover(120, 205, 255);
}  // namespace

namespace lilia::view {

GameView::GameView(sf::RenderWindow &window, bool topIsBot, bool bottomIsBot)
    : m_window(window),
      m_board_view(),
      m_piece_manager(m_board_view),
      m_highlight_manager(m_board_view),
      m_chess_animator(m_board_view, m_piece_manager),
      m_promotion_manager(),
      m_eval_bar(),
      m_move_list(),
      m_top_player(),
      m_bottom_player(),
      m_modal(),
      m_particles(),
      m_show_eval_bar(true) {
  // cursors
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

  // players
  PlayerInfo topInfo = topIsBot
                           ? getBotConfig(BotType::Lilia).info
                           : PlayerInfo{"Challenger", 0, constant::STR_FILE_PATH_ICON_CHALLENGER};
  PlayerInfo bottomInfo =
      bottomIsBot ? getBotConfig(BotType::Lilia).info
                  : PlayerInfo{"Challenger", 0, constant::STR_FILE_PATH_ICON_CHALLENGER};

  m_top_player.setInfo(topInfo);
  m_bottom_player.setInfo(bottomInfo);

  // board orientation
  m_board_view.setFlipped(bottomIsBot && !topIsBot);

  // initial layout
  layout(m_window.getSize().x, m_window.getSize().y);

  // theme font for modals (same face as the rest of UI)
  m_modal.loadFont(constant::STR_FILE_PATH_FONT);

  m_ui_font.loadFromFile(constant::STR_FILE_PATH_FONT);
  m_ui_font.setSmooth(false);
  m_eval_toggle_text.setFont(m_ui_font);
  m_eval_toggle_text.setCharacterSize(16);
}

void GameView::init(const std::string &fen) {
  m_board_view.init();
  m_piece_manager.initFromFen(fen);
  m_move_list.clear();
  m_eval_bar.reset();
  m_move_list.setFen(fen);
}

void GameView::update(float dt) {
  m_chess_animator.updateAnimations(dt);
  m_particles.update(dt);
}

void GameView::updateEval(int eval) {
  m_eval_bar.update(eval);
}

void GameView::render() {
  // left stack
  if (m_show_eval_bar) m_eval_bar.render(m_window);

  // eval bar toggle button
  sf::Vector2i mp = sf::Mouse::getPosition(m_window);
  sf::Vector2f mpos = m_window.mapPixelToCoords(mp);
  bool hov = m_eval_toggle_bounds.contains(mpos.x, mpos.y);

  sf::RectangleShape bg({m_eval_toggle_bounds.width, m_eval_toggle_bounds.height});
  bg.setPosition(m_eval_toggle_bounds.left, m_eval_toggle_bounds.top);
  bg.setFillColor(hov ? colHoverBG : colHeaderBG);
  bg.setOutlineThickness(1.f);
  bg.setOutlineColor(hov ? sf::Color(140, 200, 240, 90) : colBorder);
  m_window.draw(bg);

  m_eval_toggle_text.setString(m_show_eval_bar ? "Hide" : "Show");
  m_eval_toggle_text.setFillColor(hov ? colAccentHover : colText);
  auto tb = m_eval_toggle_text.getLocalBounds();
  m_eval_toggle_text.setOrigin(tb.left + tb.width / 2.f, tb.top + tb.height / 2.f);
  m_eval_toggle_text.setPosition(m_eval_toggle_bounds.left + m_eval_toggle_bounds.width / 2.f,
                                 m_eval_toggle_bounds.top + m_eval_toggle_bounds.height / 2.f);
  m_window.draw(m_eval_toggle_text);

  // board + pieces + overlays
  m_board_view.renderBoard(m_window);
  m_top_player.render(m_window);
  m_bottom_player.render(m_window);
  m_highlight_manager.renderSelect(m_window);
  m_chess_animator.renderHighlightLevel(m_window);
  m_highlight_manager.renderHover(m_window);
  m_piece_manager.renderPieces(m_window, m_chess_animator);
  m_highlight_manager.renderAttack(m_window);
  m_chess_animator.render(m_window);

  // right stack
  m_move_list.render(m_window);

  // modals (overlay → confetti → panel)
  if (m_modal.isResignOpen() || m_modal.isGameOverOpen()) {
    m_modal.drawOverlay(m_window);
    if (m_modal.isGameOverOpen()) {
      m_particles.render(m_window);
    }
    m_modal.drawPanel(m_window);
  }
}

void GameView::addMove(const std::string &move) {
  m_move_list.addMove(move);
}

void GameView::addResult(const std::string &result) {
  m_move_list.addResult(result);
  m_eval_bar.setResult(result);
}

void GameView::selectMove(std::size_t moveIndex) {
  m_move_list.setCurrentMove(moveIndex);
}

void GameView::setBoardFen(const std::string &fen) {
  m_chess_animator.cancelAll();
  m_piece_manager.removeAll();
  m_piece_manager.initFromFen(fen);
  m_highlight_manager.clearAllHighlights();
  m_move_list.setFen(fen);
}

void GameView::updateFen(const std::string &fen) { m_move_list.setFen(fen); }

void GameView::resetBoard() {
  m_piece_manager.removeAll();
  init();
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

// ---------- Modals (delegated to ModalView) ----------
void GameView::showResignPopup() {
  auto center = m_board_view.getPosition();
  m_modal.showResign(m_window.getSize(), {center.x, center.y});
}

void GameView::hideResignPopup() {
  m_modal.hideResign();
}

bool GameView::isResignPopupOpen() const {
  return m_modal.isResignOpen();
}

bool GameView::isOnResignYes(core::MousePos mousePos) const {
  return m_modal.hitResignYes({static_cast<float>(mousePos.x), static_cast<float>(mousePos.y)});
}

bool GameView::isOnResignNo(core::MousePos mousePos) const {
  return m_modal.hitResignNo({static_cast<float>(mousePos.x), static_cast<float>(mousePos.y)});
}

void GameView::showGameOverPopup(const std::string &msg) {
  auto center = m_board_view.getPosition();
  m_modal.showGameOver(msg, {center.x, center.y});
  if (msg.find("won") != std::string::npos) {
    m_particles.emitConfetti(center, static_cast<float>(constant::WINDOW_PX_SIZE), 200);
  }
}

void GameView::hideGameOverPopup() {
  m_modal.hideGameOver();
  m_particles.clear();
}

bool GameView::isGameOverPopupOpen() const {
  return m_modal.isGameOverOpen();
}

bool GameView::isOnNewBot(core::MousePos mousePos) const {
  return m_modal.hitNewBot({static_cast<float>(mousePos.x), static_cast<float>(mousePos.y)});
}

bool GameView::isOnRematch(core::MousePos mousePos) const {
  return m_modal.hitRematch({static_cast<float>(mousePos.x), static_cast<float>(mousePos.y)});
}

// ---------- Input helpers ----------
core::Square GameView::mousePosToSquare(core::MousePos mousePos) const {
  return m_board_view.mousePosToSquare(mousePos);
}

core::MousePos GameView::clampPosToBoard(core::MousePos mousePos,
                                         Entity::Position pieceSize) const {
  return m_board_view.clampPosToBoard(mousePos, pieceSize);
}

void GameView::setPieceToMouseScreenPos(core::Square pos, core::MousePos mousePos) {
  auto size = getPieceSize(pos);
  m_piece_manager.setPieceToScreenPos(pos, clampPosToBoard(mousePos, size));
}

void GameView::setPieceToSquareScreenPos(core::Square from, core::Square to) {
  m_piece_manager.setPieceToSquareScreenPos(from, to);
}

// ---------- Cursors ----------
void GameView::setDefaultCursor() {
  m_window.setMouseCursor(m_cursor_default);
}
void GameView::setHandOpenCursor() {
  m_window.setMouseCursor(m_cursor_hand_open);
}
void GameView::setHandClosedCursor() {
  m_window.setMouseCursor(m_cursor_hand_closed);
}

// ---------- Board info ----------
sf::Vector2u GameView::getWindowSize() const {
  return m_window.getSize();
}

Entity::Position GameView::getPieceSize(core::Square pos) const {
  return m_piece_manager.getPieceSize(pos);
}

void GameView::toggleBoardOrientation() {
  m_board_view.toggleFlipped();
}

bool GameView::isOnFlipIcon(core::MousePos mousePos) const {
  return m_board_view.isOnFlipIcon(mousePos);
}

void GameView::toggleEvalBarVisibility() { m_show_eval_bar = !m_show_eval_bar; }

bool GameView::isOnEvalToggle(core::MousePos mousePos) const {
  return m_eval_toggle_bounds.contains(static_cast<float>(mousePos.x),
                                       static_cast<float>(mousePos.y));
}

// ---------- Pieces / Highlights ----------
bool GameView::hasPieceOnSquare(core::Square pos) const {
  return m_piece_manager.hasPieceOnSquare(pos);
}

bool GameView::isSameColorPiece(core::Square sq1, core::Square sq2) const {
  return m_piece_manager.isSameColor(sq1, sq2);
}

core::PieceType GameView::getPieceType(core::Square pos) const {
  return m_piece_manager.getPieceType(pos);
}

core::Color GameView::getPieceColor(core::Square pos) const {
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

// ---------- Animations ----------
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

void GameView::playPromotionSelectAnim(core::Square promSq, core::Color c) {
  m_chess_animator.promotionSelect(promSq, m_promotion_manager, c);
}

void GameView::playPiecePlaceHolderAnimation(core::Square sq) {
  m_chess_animator.piecePlaceHolder(sq);
}

void GameView::endAnimation(core::Square sq) {
  m_chess_animator.end(sq);
}

// ---------- Layout ----------
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

  float btnW = static_cast<float>(constant::EVAL_BAR_WIDTH);
  float btnH = 26.f;
  float toggleY = vMargin + static_cast<float>(constant::WINDOW_PX_SIZE) +
                  (static_cast<float>(constant::SIDE_MARGIN) - btnH) / 2.f;
  m_eval_toggle_bounds =
      sf::FloatRect(evalCenterX - btnW / 2.f, toggleY, btnW, btnH);

  float moveListX = hMargin + static_cast<float>(constant::EVAL_BAR_WIDTH + constant::SIDE_MARGIN +
                                                 constant::WINDOW_PX_SIZE + constant::SIDE_MARGIN);
  m_move_list.setPosition({moveListX, vMargin});
  m_move_list.setSize(constant::MOVE_LIST_WIDTH, constant::WINDOW_PX_SIZE);

  float boardLeft = boardCenterX - static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f;
  float boardTop = boardCenterY - static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f;

  // player badges
  m_top_player.setPositionClamped({boardLeft + 5.f, boardTop - 45.f}, m_window.getSize());
  m_bottom_player.setPositionClamped(
      {boardLeft + 5.f, boardTop + static_cast<float>(constant::WINDOW_PX_SIZE) + 15.f},
      m_window.getSize());

  // keep modal centered on window/board changes
  m_modal.onResize(m_window.getSize(), m_board_view.getPosition());
}

}  // namespace lilia::view
