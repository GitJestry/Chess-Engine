#pragma once

#include <memory>
#include <utility>

#include <SFML/Window/Event.hpp>

#include "../model/chess_game.hpp"
#include "../view/audio/sound_manager.hpp"
#include "../view/game_view.hpp"
#include "game_manager.hpp"  
#include "input_manager.hpp"

namespace lilia::controller {

class GameController {
 public:
  
  explicit GameController(view::GameView& gView, model::ChessGame& game);

  
  void update(float dt);

  
  void handleEvent(const sf::Event& event);

  
  void render();

  
  
  void startGame(core::Color playerColor, const std::string& fen = core::START_FEN,
                 bool vsBot = true);

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
