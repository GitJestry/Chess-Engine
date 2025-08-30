#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Cursor.hpp>

namespace lilia::view {

class CursorManager {
 public:
  explicit CursorManager(sf::RenderWindow& window);

  void setDefault();
  void setHandOpen();
  void setHandClosed();

 private:
  sf::RenderWindow& m_window;
  sf::Cursor m_cursor_default;
  sf::Cursor m_cursor_hand_open;
  sf::Cursor m_cursor_hand_closed;
};

}  // namespace lilia::view

