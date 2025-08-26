#pragma once

#include <memory>
#include <utility>

// Forward declaration to avoid heavy SFML header
namespace sf {
class Event;
}

// Project headers
#include "../chess_types.hpp"
#include "../constants.hpp"
#include "../view/audio/sound_manager.hpp"
#include "../view/game_view.hpp"
#include "input_manager.hpp"

namespace lilia::model {
class ChessGame;
struct Move;
}  // namespace lilia::model

namespace lilia::controller {
class GameManager;
}

namespace lilia::controller {

class GameController {
 public:
  
  explicit GameController(view::GameView& gView, model::ChessGame& game);

  
  void update(float dt);

  
  void handleEvent(const sf::Event& event);

  
  void render();

  // game_controller.hpp (in public:)
  /**
   * @brief Startet ein Spiel über den internen GameManager.
   * @param playerColor Farbe des menschlichen Spielers (default: White).
   * @param fen Start-FEN (default: START_FEN).
   * @param vsBot true = Gegner ist Bot, false = menschlicher Gegner.
   * @param thinkTimeMs Zeit in Millisekunden, die der Bot maximal denken darf.
   * @param depth Suchtiefe für den Bot.
   */

  void startGame(core::Color playerColor, const std::string& fen = core::START_FEN,
                 bool vsBot = true, int thinkTimeMs = 1000, int depth = 5);

 private:
  

  
  void onClick(core::MousePos mousePos);

  
  void onDrag(core::MousePos start, core::MousePos current);

  
  void onDrop(core::MousePos start, core::MousePos end);

  
  void selectSquare(core::Square sq);
  void deselectSquare();
  void hoverSquare(core::Square sq);
  void dehoverSquare();

  

  
  void movePieceAndClear(const model::Move& move, bool isPlayerMove, bool onClick);

  
  void snapAndReturn(core::Square sq, core::MousePos cur);

  
  void highlightLastMove();

  
  [[nodiscard]] std::vector<core::Square> getAttackSquares(core::Square pieceSQ) const;
  void showAttacks(std::vector<core::Square> att);
  [[nodiscard]] bool tryMove(core::Square a, core::Square b);
  [[nodiscard]] bool isPromotion(core::Square a, core::Square b);
  [[nodiscard]] bool isSameColor(core::Square a, core::Square b);

  
  view::GameView& m_gameView;                 
  model::ChessGame& m_chess_game;             
  InputManager m_inputManager;                
  view::sound::SoundManager m_sound_manager;  

  core::Color m_player_color = core::Color::White;
  core::Square m_promotion_square = core::NO_SQUARE;

  core::Square m_selected_sq = core::NO_SQUARE;  
  core::Square m_hover_sq = core::NO_SQUARE;     
  std::pair<core::Square, core::Square> m_lastMoveSquares = {
      core::NO_SQUARE, core::NO_SQUARE};  

  
  std::unique_ptr<GameManager> m_gameManager;
};

}  
