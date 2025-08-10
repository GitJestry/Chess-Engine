#pragma once

#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/Event.hpp>
#include <unordered_map>
#include <functional>

class InputManager
{
public:
  // Type alias for callback functions
  using Callback = std::function<void()>;

  // Registers a keyboard key with a callback function
  void bindKey(sf::Keyboard::Key key, Callback callback);

  // Registers a mouse button with a callback function
  void bindMouse(sf::Mouse::Button button, Callback callback);

  // Handles input events (call this in your main loop)
  void processEvent(const sf::Event &event);

private:
  std::unordered_map<sf::Keyboard::Key, Callback> m_keyBindings;
  std::unordered_map<sf::Mouse::Button, Callback> m_mouseBindings;
};
