#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>

#include "../constants.hpp"
#include "../controller/mousepos.hpp"
#include "animation/chess_animator.hpp"
#include "board_view.hpp"
#include "highlight_manager.hpp"
#include "piece_manager.hpp"
#include "promotion_manager.hpp"

namespace lilia::view {

class GameView {
 public:
  GameView(sf::RenderWindow& window);
  ~GameView() = default;

  void init(const std::string& fen = core::START_FEN);

  void resetBoard();

  void update(float dt);

  void render();

  [[nodiscard]] core::Square mousePosToSquare(core::MousePos mousePos) const;
  void setPieceToMouseScreenPos(core::Square pos, core::MousePos mousePos);
  void setPieceToSquareScreenPos(core::Square from, core::Square to);

  [[nodiscard]] sf::Vector2u getWindowSize() const;
  [[nodiscard]] Entity::Position getPieceSize(core::Square pos) const;

  [[nodiscard]] bool hasPieceOnSquare(core::Square pos) const;
  [[nodiscard]] bool isSameColorPiece(core::Square sq1, core::Square sq2) const;

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
                          core::PieceType promotion = core::PieceType::None);
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

 private:
  sf::RenderWindow& m_window;

  BoardView m_board_view;
  PieceManager m_piece_manager;
  HighlightManager m_highlight_manager;
  animation::ChessAnimator m_chess_animator;
  PromotionManager m_promotion_manager;
};

}  // namespace lilia::view
