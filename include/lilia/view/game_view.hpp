#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Cursor.hpp>

#include "../constants.hpp"
#include "../controller/mousepos.hpp"
#include "animation/chess_animator.hpp"
#include "board_view.hpp"
#include "entity.hpp"
#include "eval_bar.hpp"
#include "highlight_manager.hpp"
#include "move_list_view.hpp"
#include "piece_manager.hpp"
#include "promotion_manager.hpp"
#include "player_info_view.hpp"

#include <functional>

namespace lilia::view {

class GameView {
public:
  GameView(sf::RenderWindow &window, bool topIsBot, bool bottomIsBot);
  ~GameView() = default;

  void init(const std::string &fen = core::START_FEN);

  void resetBoard();

  void update(float dt);
  void updateEval(int eval);

  void render();

  void addMove(const std::string &move);
  void selectMove(std::size_t moveIndex);
  void setBoardFen(const std::string &fen);
  void scrollMoveList(float delta);
  void setBotMode(bool anyBot);

  [[nodiscard]] std::size_t getMoveIndexAt(core::MousePos mousePos) const;

  [[nodiscard]] core::Square mousePosToSquare(core::MousePos mousePos) const;
  void setPieceToMouseScreenPos(core::Square pos, core::MousePos mousePos);
  void setPieceToSquareScreenPos(core::Square from, core::Square to);

  [[nodiscard]] sf::Vector2u getWindowSize() const;
  [[nodiscard]] Entity::Position getPieceSize(core::Square pos) const;

  [[nodiscard]] bool hasPieceOnSquare(core::Square pos) const;
  [[nodiscard]] bool isSameColorPiece(core::Square sq1, core::Square sq2) const;
  [[nodiscard]] core::PieceType getPieceType(core::Square pos) const;
  [[nodiscard]] core::Color getPieceColor(core::Square pos) const;

  void addPiece(core::PieceType type, core::Color color, core::Square pos);
  void removePiece(core::Square pos);

  void highlightSquare(core::Square pos);
  void highlightAttackSquare(core::Square pos);
  void highlightCaptureSquare(core::Square pos);
  void highlightHoverSquare(core::Square pos);
  void clearHighlightSquare(core::Square pos);
  void clearHighlightHoverSquare(core::Square pos);
  void clearAllHighlights();

  void warningKingSquareAnim(core::Square ksq);
  void animationSnapAndReturn(core::Square sq, core::MousePos mousePos);
  void animationMovePiece(core::Square from, core::Square to,
                          core::Square enPSquare = core::NO_SQUARE,
                          core::PieceType promotion = core::PieceType::None,
                          std::function<void()> onComplete = {});
  void animationDropPiece(core::Square from, core::Square to,
                          core::Square enPSquare = core::NO_SQUARE,
                          core::PieceType promotion = core::PieceType::None);
  void playPromotionSelectAnim(core::Square promSq, core::Color c);
  void playPiecePlaceHolderAnimation(core::Square sq);
  void endAnimation(core::Square sq);

  bool isInPromotionSelection();
  core::PieceType getSelectedPromotion(core::MousePos mousePos);
  void removePromotionSelection();
  void showGameOver(core::GameResult res, core::Color sideToMove);

  void setDefaultCursor();
  void setHandOpenCursor();
  void setHandClosedCursor();

  void toggleBoardOrientation();
  [[nodiscard]] bool isOnFlipIcon(core::MousePos mousePos) const;

private:
  core::MousePos clampPosToBoard(core::MousePos mousePos) const noexcept;
  void layout(unsigned int width, unsigned int height);

  sf::RenderWindow &m_window;
  BoardView m_board_view;
  PieceManager m_piece_manager;
  HighlightManager m_highlight_manager;
  animation::ChessAnimator m_chess_animator;
  PromotionManager m_promotion_manager;
  sf::Cursor m_cursor_default;
  sf::Cursor m_cursor_hand_open;
  sf::Cursor m_cursor_hand_closed;
  EvalBar m_eval_bar;
  MoveListView m_move_list;
  PlayerInfoView m_top_player;
  PlayerInfoView m_bottom_player;
};

} // namespace lilia::view
