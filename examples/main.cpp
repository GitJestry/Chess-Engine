#include <SFML/Graphics.hpp>
#include <test.hpp>

int main() {

    TestClass test;
    test.printHelloWorld();

    // Create a window with the specified size and title
    sf::RenderWindow window(sf::VideoMode(1000, 600), "SFML Test");

    // Main loop
    while (window.isOpen()) {
        sf::Event event;
        // Handle events
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        // Clear the screen with a color
        window.clear(sf::Color::Blue);

        // Display the current frame
        window.display();
    }

    return 0;
}


 