#pragma once

// Forward declaration to avoid including full SFML headers
namespace sf {
class Event;
}  // namespace sf

#include <functional>
#include <optional>

#include "../core_types.hpp"

namespace lilia {

/**
 * @class InputManager
 * @brief Handles mouse input events and dispatches them to registered callbacks.
 *
 * Supported interactions:
 * - Mouse click
 * - Mouse drag
 * - Mouse drop (release)
 *
 * Uses std::function-based callbacks that can be registered by the caller.
 */
class InputManager {
 public:
  /// Callback type for mouse click events.
  using ClickCallback = std::function<void(core::MousePos)>;

  /// Callback type for mouse drag events.
  using DragCallback = std::function<void(core::MousePos start, core::MousePos current)>;

  /// Callback type for mouse drop (release) events.
  using DropCallback = std::function<void(core::MousePos start, core::MousePos end)>;

  /**
   * @brief Register callback for click events.
   * @param cb The callback to invoke when a click is detected.
   */
  void setOnClick(ClickCallback cb);

  /**
   * @brief Register callback for drag events.
   * @param cb The callback to invoke during a drag operation.
   */
  void setOnDrag(DragCallback cb);

  /**
   * @brief Register callback for drop events.
   * @param cb The callback to invoke when a drag ends.
   */
  void setOnDrop(DropCallback cb);

  /**
   * @brief Process an SFML input event and trigger callbacks accordingly.
   * @param event The event to process.
   */
  void processEvent(const sf::Event& event);

 private:
  bool m_dragging = false;                    ///< Indicates whether a drag operation is active.
  std::optional<core::MousePos> m_dragStart;  ///< Starting position of an active drag.

  ClickCallback m_onClick = nullptr;  ///< Registered click callback.
  DragCallback m_onDrag = nullptr;    ///< Registered drag callback.
  DropCallback m_onDrop = nullptr;    ///< Registered drop callback.

  /**
   * @brief Determine if a mouse release should be considered a click.
   * @param start Position where mouse was pressed.
   * @param end Position where mouse was released.
   * @param threshold Max distance (pixels) for release to count as a click.
   * @return True if the release qualifies as a click.
   */
  bool isClick(const core::MousePos& start, const core::MousePos& end, int threshold = 2) const;
};

}  // namespace lilia
