#pragma once

#include <unordered_map>

#include "../core_types.hpp"
#include "../model/chess_game.hpp"
#include "animation/animation_manager.hpp"
#include "board.hpp"
#include "piece.hpp"

namespace lilia {

class GameView {
 public:
  ~GameView() = default;
  GameView(sf::RenderWindow& window, ChessGame& game);

  // initialise the board and places the pieces according to the fen
  void init(const std::string& fen = core::START_FEN);

  void updateAnimations(float dt);

  // renders the board, pieces and highlights
  void render();

  // resets the board to the START_FEN position
  void resetBoard();

  // adds piece on the given square to the scene
  void addPiece(core::PieceType type, core::PieceColor color, core::Square pos);

  // removes piece on the given square from the scene
  void removePiece(core::Square pos);

  void animationSnapAndReturn(core::Square sq, core::MousePos mousePos);
  void animationMovePiece(core::Square from, core::Square to);
  void animationDropPiece(core::Square from, core::Square to);
  void playPlaceHolderAnimation(core::Square sq);
  void endAnimation(core::Square sq);

  core::MousePos squareToMousePos(core::Square sq);
  core::Square mousePosToSquare(core::MousePos mousePos);

  // highlights a square in the highlight color
  void hlightSquare(core::Square pos);
  // highlights a square with the attacking dot
  void hlightAttackSquare(core::Square pos);
  void hlightHoverSquare(core::Square pos);

  // Clears all attack and normal highlights
  void clearAllHlights();
  void clearHlightSquare(core::Square pos);
  void clearHlightHoverSquare(core::Square pos);

  // Displays whose turn it is
  void updateTurnIndicator(core::PieceColor activeColor);

  // Display messages like, in check, checkmate, winner, pawn promotion
  void showMessage(const std::string& message);

  bool hasPieceOnSquare(core::Square pos) const;

  void setPieceToSquareScreenPos(core::Square from, core::Square to);
  void setPieceToMouseScreenPos(core::Square pos, core::MousePos mousePos);

 private:
  // removes the piece from the starting position, and places it on the destination
  // The previous Piece on the Square destination will be removed/replaced
  void movePiece(core::Square from, core::Square to);
  Entity::Position getSquareScreenPos(core::Square pos);
  template <typename T>
  void renderEntitiesToBoard(std::unordered_map<core::Square, T>& map);

  Board m_board;
  ChessGame& m_chess_game;
  AnimationManager m_anim_manager;
  std::unordered_map<core::Square, Piece> m_pieces;
  sf::RenderWindow& m_window_ref;
  std::unordered_map<core::Square, Entity> m_hl_attack_squares;
  std::unordered_map<core::Square, Entity> m_hl_select_squares;
  std::unordered_map<core::Square, Entity> m_hl_hover_squares;
};

}  // namespace lilia
