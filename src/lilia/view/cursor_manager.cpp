#include "lilia/view/cursor_manager.hpp"

#include <SFML/Graphics/Image.hpp>

#include "lilia/view/render_constants.hpp"

namespace lilia::view {

CursorManager::CursorManager(sf::RenderWindow &window) : m_window(window) {
  m_cursor_default.loadFromSystem(sf::Cursor::Arrow);

  sf::Image openImg;
  if (openImg.loadFromFile(constant::STR_FILE_PATH_HAND_OPEN)) {
    m_cursor_hand_open.loadFromPixels(openImg.getPixelsPtr(), openImg.getSize(),
                                      {openImg.getSize().x / 3, openImg.getSize().y / 3});
  }

  sf::Image closedImg;
  if (closedImg.loadFromFile(constant::STR_FILE_PATH_HAND_CLOSED)) {
    m_cursor_hand_closed.loadFromPixels(closedImg.getPixelsPtr(), closedImg.getSize(),
                                        {closedImg.getSize().x / 3, closedImg.getSize().y / 3});
  }

  m_window.setMouseCursor(m_cursor_default);
}

void CursorManager::setDefaultCursor() { m_window.setMouseCursor(m_cursor_default); }

void CursorManager::setHandOpenCursor() { m_window.setMouseCursor(m_cursor_hand_open); }

void CursorManager::setHandClosedCursor() { m_window.setMouseCursor(m_cursor_hand_closed); }

} // namespace lilia::view

