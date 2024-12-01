#include <SFML/Graphics.hpp>
#include <board.hpp>
#include <iostream>
#include <texture_manager.hpp>

int main()
{
  // Create a window with the specified size and title
  sf::RenderWindow window(sf::VideoMode(800, 800), "Lilia", sf::Style::Titlebar | sf::Style::Close);
  Board board({400, 400});
  TextureManager manager;
  manager.loadTexture("black", sf::Color::Black); // Black with full alpha
  manager.loadTexture("white", sf::Color::White); // White with full alpha
  manager.loadTexture("transparent", sf::Color::Transparent);

  board.initialize(manager.getTexture("white"), manager.getTexture("black"), manager.getTexture("transparent"), window.getSize().x);
  // Main loop
  while (window.isOpen())
  {
    sf::Event event;
    // Handle events
    while (window.pollEvent(event))
    {

      if (event.type == sf::Event::Closed)
        window.close();
    }

    // Clear the screen with a color
    window.clear(sf::Color::Blue);
    board.draw(window);
    // Display the current frame
    window.display();
  }

  return 0;
}
