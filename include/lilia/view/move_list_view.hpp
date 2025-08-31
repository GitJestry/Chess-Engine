#pragma once

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>
#include <string>
#include <vector>

#include "entity.hpp"
#include "render_constants.hpp"

namespace lilia::view {

class MoveListView {
public:
  MoveListView();

  void setPosition(const Entity::Position &pos);
  void setSize(unsigned int width, unsigned int height);

  void addMove(const std::string &uciMove);
  void setCurrentMove(std::size_t moveIndex);
  void render(sf::RenderWindow &window) const;
  void scroll(float delta);
  void clear();

  void setBotMode(bool anyBot);

  [[nodiscard]] std::size_t getMoveIndexAt(const Entity::Position &pos) const;

private:
  sf::Font m_font;
  std::vector<std::string> m_lines;
  Entity::Position m_position{}; // Top-left position
  unsigned int m_width{constant::MOVE_LIST_WIDTH};
  unsigned int m_height{constant::WINDOW_PX_SIZE};
  float m_scroll_offset{0.f};
  std::size_t m_move_count{0};
  std::size_t m_selected_move{static_cast<std::size_t>(-1)};
  std::vector<sf::FloatRect> m_move_bounds;
  bool m_any_bot{false};
};

} // namespace lilia::view
