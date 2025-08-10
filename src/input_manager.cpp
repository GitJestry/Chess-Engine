#include <input_manager.hpp>

void InputManager::bindKey(sf::Keyboard::Key key, Callback callback) {
  m_keyBindings[key] = callback;
}

void InputManager::bindMouse(sf::Mouse::Button button, Callback callback) {
  m_mouseBindings[button] = callback;
}

void InputManager::processEvent(const sf::Event &event) {
  // Check for keyboard press events
  if (event.type == sf::Event::KeyPressed) {
    auto it = m_keyBindings.find(event.key.code);
    if (it != m_keyBindings.end()) {
      it->second();  // Call the bound function
    }
  }

  // Check for mouse button press events
  if (event.type == sf::Event::MouseButtonPressed) {
    auto it = m_mouseBindings.find(event.mouseButton.button);
    if (it != m_mouseBindings.end()) {
      it->second();  // Call the bound function
    }
  }
}
