#include <iostream>

#include "lilia/controller/game_controller.hpp"
#include "lilia/model/chess_game.hpp"
#include "lilia/view/game_view.hpp"
#include "lilia/view/texture_table.hpp"

int main() {
  lilia::view::TextureTable::getInstance().preLoad();
  sf::RenderWindow window(
      sf::VideoMode(lilia::view::constant::WINDOW_PX_SIZE, lilia::view::constant::WINDOW_PX_SIZE),
      "Lilia", sf::Style::Titlebar | sf::Style::Close);

  lilia::model::ChessGame chessgame;
  lilia::view::GameView view(window, chessgame);
  lilia::controller::GameController gController(view, chessgame);

  sf::Clock clock;

  while (window.isOpen()) {
    float dt = clock.restart().asSeconds();
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
