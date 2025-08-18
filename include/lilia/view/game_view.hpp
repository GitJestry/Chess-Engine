#pragma once

#include <SFML/Graphics.hpp>
#include <string>

#include "../core_types.hpp"
#include "../model/chess_game.hpp"
#include "animation/chess_animator.hpp"
#include "board_view.hpp"
#include "highlight_manager.hpp"
#include "piece_manager.hpp"

namespace lilia {

/**
 * @brief Facade for rendering and updating the chess game.
 *
 * GameView delegates responsibilities to specialized managers:
 * - BoardView (background, squares)
 * - PieceManager (pieces on the board)
 * - HighlightManager (move hints, hover, selections)
 * - AnimationManager (piece movements, snapping)
 * - UIOverlay (turn indicator, messages)
 */
class GameView {
 public:
  GameView(sf::RenderWindow& window, ChessGame& game);
  ~GameView() = default;

  /// Initialise the board + pieces according to given FEN
  void init(const std::string& fen = core::START_FEN);

  /// Reset to START_FEN
  void resetBoard();

  /// Update animations
  void update(float dt);

  /// Render board, pieces, highlights, UI
  void render();

  /// --- Delegated Facade Methods ---
  [[nodiscard]] core::Square mousePosToSquare(core::MousePos mousePos) const;
  void setPieceToMouseScreenPos(core::Square pos, core::MousePos mousePos);
  void setPieceToSquareScreenPos(core::Square from, core::Square to);

  [[nodiscard]] bool hasPieceOnSquare(core::Square pos) const;
  [[nodiscard]] bool isSameColorPiece(core::Square sq1, core::Square sq2) const;

  void highlightSquare(core::Square pos);
  void highlightAttackSquare(core::Square pos);
  void highlightHoverSquare(core::Square pos);
  void clearHighlightSquare(core::Square pos);
  void clearHighlightHoverSquare(core::Square pos);
  void clearAllHighlights();

  void animationSnapAndReturn(core::Square sq, core::MousePos mousePos);
  void animationMovePiece(core::Square from, core::Square to);
  void animationDropPiece(core::Square from, core::Square to);
  void playPiecePlaceHolderAnimation(core::Square sq);
  void endAnimation(core::Square sq);

  void updateTurnIndicator(core::PieceColor activeColor);
  void showMessage(const std::string& message);

 private:
  sf::RenderWindow& m_window;
  ChessGame& m_game;

  BoardView m_board_view;
  PieceManager m_piece_manager;
  HighlightManager m_highlight_manager;
  ChessAnimator m_chess_animator;
};

}  // namespace lilia
