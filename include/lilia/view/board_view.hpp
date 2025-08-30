#pragma once

#include <string>

#include "board.hpp"
#include "../controller/mousepos.hpp"

namespace lilia::view {

class BoardView {
 public:
  BoardView();

  void init();
  void renderBoard(sf::RenderWindow& window);
  [[nodiscard]] Entity::Position getSquareScreenPos(core::Square sq) const;
  void toggleFlipped();
  void setFlipped(bool flipped);
  [[nodiscard]] bool isFlipped() const;
  [[nodiscard]] bool isOnFlipIcon(core::MousePos mousePos) const;

  void setPosition(const Entity::Position& pos);
  [[nodiscard]] Entity::Position getPosition() const;

 private:
  Board m_board;
  Entity m_flip_icon;
  bool m_flipped{false};
};

}  // namespace lilia::view
