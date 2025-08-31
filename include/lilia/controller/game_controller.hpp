#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Forward declaration to avoid heavy SFML header
namespace sf {
class Event;
}

// Project headers
#include "../chess_types.hpp"
#include "../constants.hpp"
#include "../model/move.hpp"
#include "../view/audio/sound_manager.hpp"
#include "../view/game_view.hpp"
#include "input_manager.hpp"

namespace lilia::model {
class ChessGame;
struct Move;
}  // namespace lilia::model

namespace lilia::controller {
class GameManager;

struct MoveView {
  model::Move move;
  core::Color moverColor;
  core::PieceType capturedType;
  view::sound::Effect sound;
};

class GameController {
 public:
  explicit GameController(view::GameView& gView, model::ChessGame& game);
  ~GameController();

  void update(float dt);

  void handleEvent(const sf::Event& event);

  void render();

  // game_controller.hpp (in public:)
  /**
   * @brief Startet ein Spiel über den internen GameManager.
   * @param fen Start-FEN (default: START_FEN).
   * @param whiteIsBot true, falls der weiße Spieler ein Bot ist.
   * @param blackIsBot true, falls der schwarze Spieler ein Bot ist.
   * @param thinkTimeMs Zeit in Millisekunden, die der Bot maximal denken darf.
   * @param depth Suchtiefe für den Bot.
   */

  void startGame(const std::string& fen = core::START_FEN, bool whiteIsBot = false,
                 bool blackIsBot = true, int thinkTimeMs = 1000, int depth = 5);

 private:
  bool isHumanPiece(core::Square sq) const;
  bool hasCurrentLegalMove(core::Square from, core::Square to) const;

  void onMouseMove(core::MousePos pos);
  void onMousePressed(core::MousePos pos);
  void onMouseReleased(core::MousePos pos);
  void onClick(core::MousePos mousePos);

  void onDrag(core::MousePos start, core::MousePos current);

  void onDrop(core::MousePos start, core::MousePos end);

  void selectSquare(core::Square sq);
  void deselectSquare();
  void hoverSquare(core::Square sq);
  void dehoverSquare();
  void clearPremove();

  void movePieceAndClear(const model::Move& move, bool isPlayerMove, bool onClick);

  void snapAndReturn(core::Square sq, core::MousePos cur);
  void highlightLastMove();

  [[nodiscard]] std::vector<core::Square> getAttackSquares(core::Square pieceSQ) const;
  void showAttacks(std::vector<core::Square> att);
  [[nodiscard]] bool tryMove(core::Square a, core::Square b);
  [[nodiscard]] bool isPromotion(core::Square a, core::Square b);
  [[nodiscard]] bool isSameColor(core::Square a, core::Square b);

  // ---------------- Members ----------------
  view::GameView& m_game_view;                ///< Responsible for rendering.
  model::ChessGame& m_chess_game;             ///< Game model containing rules and state.
  InputManager m_input_manager;               ///< Handles raw input processing.
  view::sound::SoundManager m_sound_manager;  ///< Handles sfx and music

  core::Square m_promotion_square = core::NO_SQUARE;

  bool m_dragging = false;
  bool m_mouse_down = false;
  bool m_has_pending_auto_move = false;

  core::Square m_drag_from = core::NO_SQUARE;
  bool m_preview_active = false;
  core::Square m_prev_selected_before_preview = core::NO_SQUARE;
  bool m_selection_changed_on_press = false;

  core::Square m_premove_from = core::NO_SQUARE;
  core::Square m_premove_to = core::NO_SQUARE;
  core::Square m_pending_from = core::NO_SQUARE;
  core::Square m_pending_to = core::NO_SQUARE;

  core::Square m_selected_sq = core::NO_SQUARE;  ///< Currently selected square.
  core::Square m_hover_sq = core::NO_SQUARE;     ///< Currently hovered square.
  std::pair<core::Square, core::Square> m_last_move_squares = {
      core::NO_SQUARE, core::NO_SQUARE};  ///< Last executed move (from -> to).

  // ---------------- New: GameManager ----------------
  std::unique_ptr<GameManager> m_game_manager;
  std::atomic<int> m_eval_cp{0};

  std::vector<std::string> m_fen_history;
  std::size_t m_fen_index{0};
  std::vector<MoveView> m_move_history;
};

}  // namespace lilia::controller
