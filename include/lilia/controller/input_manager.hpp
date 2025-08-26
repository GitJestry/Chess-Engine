#pragma once

namespace sf {
class Event;
}

#include <chrono>
#include <functional>
#include <optional>

#include "../controller/mousepos.hpp"

namespace lilia::controller {

class InputManager {
public:
  using ClickCallback = std::function<void(core::MousePos)>;

  using DragCallback =
      std::function<void(core::MousePos start, core::MousePos current)>;

  using DropCallback =
      std::function<void(core::MousePos start, core::MousePos end)>;

  void setOnClick(ClickCallback cb);
  void setOnDrag(DragCallback cb);
  void setOnDrop(DropCallback cb);

  void processEvent(const sf::Event &event);

private:
  bool m_dragging = false; ///< Indicates whether a drag operation is active.
  std::optional<core::MousePos>
      m_drag_start; ///< Starting position of an active drag.
  std::chrono::steady_clock::time_point
      m_drag_start_time; ///< Timestamp when the current drag started.
  bool m_drag_triggered =
      false; ///< Indicates whether on_drag has been invoked.

  ClickCallback m_on_click = nullptr; ///< Registered click callback.
  DragCallback m_on_drag = nullptr;   ///< Registered drag callback.
  DropCallback m_on_drop = nullptr;   ///< Registered drop callback.

  [[nodiscard]] bool isClick(const core::MousePos &start,
                             const core::MousePos &end,
                             int threshold = 2) const;
};

} // namespace lilia::controller
