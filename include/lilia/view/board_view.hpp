#pragma once

#include <string>

#include "../controller/mousepos.hpp"
#include "board.hpp"
#include "color_palette_manager.hpp"
#include "entity.hpp"

namespace lilia::view {

class BoardView {
 public:
  BoardView();
  ~BoardView();

  void init();
  void renderBoard(sf::RenderWindow& window);
  [[nodiscard]] Entity::Position getSquareScreenPos(core::Square sq) const;
  void toggleFlipped();
  void setFlipped(bool flipped);
  [[nodiscard]] bool isFlipped() const;
  [[nodiscard]] bool isOnFlipIcon(core::MousePos mousePos) const;
  core::MousePos clampPosToBoard(core::MousePos mousePos,
                                 Entity::Position pieceSize = {0.f, 0.f}) const noexcept;
  [[nodiscard]] core::Square mousePosToSquare(core::MousePos mousePos) const;

  void setPosition(const Entity::Position& pos);
  [[nodiscard]] Entity::Position getPosition() const;

 private:
  void onPaletteChanged();

  Board m_board;
  Entity::Position m_flip_pos{};
  float m_flip_size{0.f};
  bool m_flipped{false};

  ColorPaletteManager::ListenerID m_paletteListener{0};
};

}  // namespace lilia::view
