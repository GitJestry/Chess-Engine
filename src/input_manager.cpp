#include <input_manager.hpp>

void InputManager::bindKey(sf::Keyboard::Key key, Callback callback)
{
  keyBindings[key] = callback;
}

void InputManager::bindMouse(sf::Mouse::Button button, Callback callback)
{
  mouseBindings[button] = callback;
}

void InputManager::processEvent(const sf::Event &event)
{
  // Check for keyboard press events
  if (event.type == sf::Event::KeyPressed)
  {
    auto it = keyBindings.find(event.key.code);
    if (it != keyBindings.end())
    {
      it->second(); // Call the bound function
    }
  }

  // Check for mouse button press events
  if (event.type == sf::Event::MouseButtonPressed)
  {
    auto it = mouseBindings.find(event.mouseButton.button);
    if (it != mouseBindings.end())
    {
      it->second(); // Call the bound function
    }
  }
}
