#pragma once

// Standard library
#include <utility>
#include <vector>

// External libraries
#include <SFML/Window/Event.hpp>

// Project headers
#include "../model/chess_game.hpp"
#include "../view/audio/sound_manager.hpp"
#include "../view/game_view.hpp"
#include "input_manager.hpp"

namespace lilia::controller {

/**
 * @class GameController
 * @brief Orchestrates the flow of the chess game.
 *
 * Responsibilities:
 * - Updating game state (timers, moves, AI turns, animations)
 * - Handling user input (mouse clicks, drags, drops)
 * - Rendering the board and UI via GameView
 */
class GameController {
 public:
  /**
   * @brief Construct a new GameController.
   * @param gView Reference to the GameView for rendering.
   * @param game Reference to the ChessGame model.
   */
  explicit GameController(view::GameView& gView, model::ChessGame& game);

  /**
   * @brief Update loop independent from user inputs.
   * @param dt Delta time since last frame in seconds.
   *
   * Handles:
   * - Bot moves
   * - Animations
   * - Game status updates (check, checkmate, timers)
   */
  void update(float dt);

  /**
   * @brief Process SFML input events.
   * @param event The SFML event to handle.
   */
  void handleEvent(const sf::Event& event);

  /**
   * @brief Render the current game state through GameView.
   */
  void render();

 private:
  // ---------------- Input handlers ----------------

  /**
   * @brief Handle mouse click event on the board.
   * @param mousePos Position of the mouse click in board coordinates.
   */
  void onClick(core::MousePos mousePos);

  /**
   * @brief Handle mouse drag event across the board.
   * @param start Starting mouse position.
   * @param current Current mouse position during drag.
   */
  void onDrag(core::MousePos start, core::MousePos current);

  /**
   * @brief Handle mouse drop (release) event on the board.
   * @param start Starting square of the dragged piece.
   * @param end Target square of the dropped piece.
   */
  void onDrop(core::MousePos start, core::MousePos end);

  // ---------------- Selection & hover logic ----------------
  void selectSquare(core::Square sq);
  void deselectSquare();
  void hoverSquare(core::Square sq);
  void dehoverSquare();

  // ---------------- Move and animation logic ----------------

  /**
   * @brief Execute a piece move and animate the transition.
   * @param from Origin square.
   * @param to Destination square.
   * @param onClick If true, triggered directly by user input.
   */
  void movePieceAndClear(core::Square from, core::Square to, bool onClick);

  /**
   * @brief Visually snap a piece to the mouse and return it.
   * @param sq The square of the piece.
   * @param cur Current mouse position.
   */
  void snapAndReturn(core::Square sq, core::MousePos cur);

  /**
   * @brief Highlight the most recent move on the board.
   */
  void highlightLastMove();

  // ---------------- Helpers ----------------
  [[nodiscard]] std::vector<core::Square> getAttackSquares(core::Square pieceSQ) const;
  void showAttacks(std::vector<core::Square> att);
  [[nodiscard]] bool tryMove(core::Square a, core::Square b);
  [[nodiscard]] bool isSameColor(core::Square a, core::Square b);

  // ---------------- Members ----------------
  view::GameView& m_gameView;                 ///< Responsible for rendering.
  model::ChessGame& m_chess_game;             ///< Game model containing rules and state.
  InputManager m_inputManager;                ///< Handles raw input processing.
  view::sound::SoundManager m_sound_manager;  ///< Handles sfx and music

  core::Square m_selected_sq = core::NO_SQUARE;  ///< Currently selected square.
  core::Square m_hover_sq = core::NO_SQUARE;     ///< Currently hovered square.
  std::pair<core::Square, core::Square> m_lastMoveSquares = {
      core::NO_SQUARE, core::NO_SQUARE};  ///< Last executed move (from -> to).
};

}  // namespace lilia::controller
