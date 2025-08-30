#include "lilia/view/cursor_manager.hpp"

#include <SFML/Graphics/Image.hpp>

#include "lilia/view/render_constants.hpp"

namespace lilia::view {

CursorManager::CursorManager(sf::RenderWindow& window) : m_window(window) {
  m_cursor_default.loadFromSystem(sf::Cursor::Arrow);

  sf::Image openImg;
  if (openImg.loadFromFile(constant::STR_FILE_PATH_HAND_OPEN)) {
    m_cursor_hand_open.loadFromPixels(openImg.getPixelsPtr(), openImg.getSize(),
                                      {openImg.getSize().x / 2, openImg.getSize().y / 2});
  }

  sf::Image closedImg;
  if (closedImg.loadFromFile(constant::STR_FILE_PATH_HAND_CLOSED)) {
    m_cursor_hand_closed.loadFromPixels(closedImg.getPixelsPtr(), closedImg.getSize(),
                                        {closedImg.getSize().x / 2, closedImg.getSize().y / 2});
  }
}

void CursorManager::setDefault() { m_window.setMouseCursor(m_cursor_default); }

void CursorManager::setHandOpen() { m_window.setMouseCursor(m_cursor_hand_open); }

void CursorManager::setHandClosed() { m_window.setMouseCursor(m_cursor_hand_closed); }

}  // namespace lilia::view

