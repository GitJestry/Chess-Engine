#include "lilia/app/app.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>

#include "lilia/controller/game_controller.hpp"
#include "lilia/engine/engine.hpp"
#include "lilia/model/chess_game.hpp"
#include "lilia/view/game_view.hpp"
#include "lilia/view/start_screen.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::app {

int App::run() {
  engine::Engine::init();
  lilia::view::TextureTable::getInstance().preLoad();

  sf::RenderWindow window(
      sf::VideoMode(lilia::view::constant::WINDOW_TOTAL_WIDTH,
                    lilia::view::constant::WINDOW_TOTAL_HEIGHT),
      "Lilia", sf::Style::Titlebar | sf::Style::Close);

  while (window.isOpen()) {
    lilia::view::StartScreen startScreen(window);
    auto cfg = startScreen.run();
    m_white_is_bot = cfg.whiteIsBot;
    m_black_is_bot = cfg.blackIsBot;

    lilia::controller::GameController::NextAction action =
        lilia::controller::GameController::NextAction::None;

    do {
      action = lilia::controller::GameController::NextAction::None;
      lilia::model::ChessGame chessGame;
      lilia::view::GameView gameView(window, m_black_is_bot, m_white_is_bot);
      lilia::controller::GameController gameController(gameView, chessGame);

      gameController.startGame(m_start_fen, m_white_is_bot, m_black_is_bot,
                               m_thinkTimeMs, m_searchDepth);

      sf::Clock clock;
      while (window.isOpen() &&
             gameController.getNextAction() ==
                 lilia::controller::GameController::NextAction::None) {
        float deltaSeconds = clock.restart().asSeconds();
        sf::Event event;
        while (window.pollEvent(event)) {
          if (event.type == sf::Event::Closed) window.close();
          gameController.handleEvent(event);
        }
        gameController.update(deltaSeconds);
        window.clear(sf::Color{48, 46, 43});
        gameController.render();
        window.display();
      }

      if (!window.isOpen()) return 0;

      action = gameController.getNextAction();

    } while (action ==
             lilia::controller::GameController::NextAction::Rematch &&
             window.isOpen());

    if (action != lilia::controller::GameController::NextAction::NewBot)
      break;
  }

  return 0;
}

}  // namespace lilia::app

