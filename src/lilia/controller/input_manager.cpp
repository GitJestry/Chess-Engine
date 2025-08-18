
#include "lilia/controller/input_manager.hpp"

#include <SFML/Window/Event.hpp>
#include <cmath>

namespace lilia::controller {

void InputManager::setOnClick(ClickCallback cb) {
  m_onClick = std::move(cb);
}

void InputManager::setOnDrag(DragCallback cb) {
  m_onDrag = std::move(cb);
}

void InputManager::setOnDrop(DropCallback cb) {
  m_onDrop = std::move(cb);
}

void InputManager::processEvent(const sf::Event& event) {
  switch (event.type) {
    case sf::Event::MouseButtonPressed:
      if (event.mouseButton.button == sf::Mouse::Left) {
        m_dragStart = core::MousePos(event.mouseButton.x, event.mouseButton.y);
        m_dragging = true;
      }
      break;

    case sf::Event::MouseMoved:
      if (m_dragging && m_dragStart) {
        core::MousePos currentPos(event.mouseMove.x, event.mouseMove.y);
        if (m_onDrag) m_onDrag(m_dragStart.value(), currentPos);
      }
      break;

    case sf::Event::MouseButtonReleased:
      if (event.mouseButton.button == sf::Mouse::Left && m_dragging && m_dragStart) {
        core::MousePos dropPos(event.mouseButton.x, event.mouseButton.y);

        if (isClick(m_dragStart.value(), dropPos)) {
          if (m_onClick) m_onClick(dropPos);
        } else {
          if (m_onDrop) m_onDrop(m_dragStart.value(), dropPos);
        }

        m_dragStart.reset();
        m_dragging = false;
      }
      break;

    default:
      break;
  }
}

[[nodiscard]] bool InputManager::isClick(const core::MousePos& start, const core::MousePos& end,
                                         int threshold) const {
  int dx = end.x - start.x;
  int dy = end.y - start.y;
  return (dx * dx + dy * dy) <= (threshold * threshold);
}

}  // namespace lilia::controller
