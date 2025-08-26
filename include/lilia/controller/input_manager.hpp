#pragma once

namespace sf {
class Event;
}  

#include <functional>
#include <optional>

#include "../controller/mousepos.hpp"

namespace lilia::controller {

class InputManager {
 public:
  
  using ClickCallback = std::function<void(core::MousePos)>;

  
  using DragCallback = std::function<void(core::MousePos start, core::MousePos current)>;

  
  using DropCallback = std::function<void(core::MousePos start, core::MousePos end)>;

  
  void setOnClick(ClickCallback cb);

  
  void setOnDrag(DragCallback cb);

  
  void setOnDrop(DropCallback cb);

  
  void processEvent(const sf::Event& event);

 private:
  bool m_dragging = false;                    
  std::optional<core::MousePos> m_dragStart;  

  ClickCallback m_onClick = nullptr;  
  DragCallback m_onDrag = nullptr;    
  DropCallback m_onDrop = nullptr;    

  
  [[nodiscard]] bool isClick(const core::MousePos& start, const core::MousePos& end,
                             int threshold = 2) const;
};

}  
