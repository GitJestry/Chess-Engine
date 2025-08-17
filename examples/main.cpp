#include <SFML/Graphics.hpp>
#include <iostream>

#include "lilia/controller/game_controller.hpp"
#include "lilia/model/chess_game.hpp"
#include "lilia/view/game_view.hpp"
#include "lilia/view/texture_table.hpp"

int main() {
  sf::RenderWindow window(sf::VideoMode(lilia::core::WINDOW_PX_SIZE, lilia::core::WINDOW_PX_SIZE),
                          "Lilia", sf::Style::Titlebar | sf::Style::Close);
  lilia::TextureTable::getInstance().preLoad();

  lilia::ChessGame chessgame;
  lilia::GameView view(window, chessgame);
  lilia::GameController gController(view, chessgame);

  sf::Clock clock;

  while (window.isOpen()) {
    float dt = clock.restart().asSeconds();  // Sekunden seit letztem Frame
    sf::Event event;
    while (window.pollEvent(event)) {
      if (event.type == sf::Event::Closed) window.close();
      gController.handleEvent(event);
    }
    gController.update(dt);
    window.clear(sf::Color::Blue);
    gController.render();
    window.display();
  }

  return 0;
}
