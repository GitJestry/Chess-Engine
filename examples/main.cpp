#include <SFML/Graphics.hpp>
#include <constants.hpp>
#include <input_manager.hpp>
#include <iostream>

#include "visuals/game_view.hpp"

int main() {
  // Create a window with the specified size and title
  sf::RenderWindow window(sf::VideoMode(WINDOW_SIZE, WINDOW_SIZE), "Lilia",
                          sf::Style::Titlebar | sf::Style::Close);

  InputManager inputManager;
  GameView game(window);
  game.init();

  // Bind some keys and mouse buttons
  inputManager.bindKey(sf::Keyboard::A, []() { std::cout << "Key A pressed!" << std::endl; });

  inputManager.bindMouse(sf::Mouse::Left,
                         []() { std::cout << "Left mouse button clicked!" << std::endl; });
  //  Main loop
  while (window.isOpen()) {
    sf::Event event;
    // Handle events
    while (window.pollEvent(event)) {
      if (event.type == sf::Event::Closed) window.close();

      inputManager.processEvent(event);
    }

    // Clear the screen with a color
    window.clear(sf::Color::Blue);
    game.renderBoard();
    // Display the current frame
    window.display();
  }

  return 0;
}
