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
  ~MoveListView();

  void setPosition(const Entity::Position &pos);
  void setSize(unsigned int width, unsigned int height);
  void setFen(const std::string &fen);

  void addMove(const std::string &uciMove);
  void addResult(const std::string &result);
  void setCurrentMove(std::size_t moveIndex);
  void render(sf::RenderWindow &window) const;
  void scroll(float delta);
  void clear();

  void setBotMode(bool anyBot);

  [[nodiscard]] std::size_t getMoveIndexAt(const Entity::Position &pos) const;

  enum class Option { None, Resign, Prev, Next, Settings, NewBot, Rematch, ShowFen };
  [[nodiscard]] Option getOptionAt(const Entity::Position &pos) const;
  void setGameOver(bool over);

 private:
  sf::Font m_font;
  std::vector<std::string> m_lines;
  std::string m_result;
  Entity::Position m_position{}; // Top-left position
  unsigned int m_width{constant::MOVE_LIST_WIDTH};
  unsigned int m_height{constant::WINDOW_PX_SIZE};
  float m_option_height{0.f};
  float m_scroll_offset{0.f};
  std::size_t m_move_count{0};
  std::size_t m_selected_move{static_cast<std::size_t>(-1)};
  std::vector<sf::FloatRect> m_move_bounds;
  bool m_any_bot{false};
  bool m_game_over{false};
  std::string m_fen_str{};

  // Icons in bottom option field
  mutable Entity m_icon_resign;
  mutable Entity m_icon_prev;
  mutable Entity m_icon_next;
  mutable Entity m_icon_settings;
  mutable Entity m_icon_new_bot;
  mutable Entity m_icon_rematch;
  sf::FloatRect m_bounds_resign{};
  sf::FloatRect m_bounds_prev{};
  sf::FloatRect m_bounds_next{};
  sf::FloatRect m_bounds_settings{};
  sf::FloatRect m_bounds_new_bot{};
  sf::FloatRect m_bounds_rematch{};
  sf::FloatRect m_bounds_fen_icon{};

  // Palette
  ColorPaletteManager::ListenerID m_paletteListener{0};
  sf::Color m_toastTextColor{constant::COL_TEXT};

  void onPaletteChanged();
};

} // namespace lilia::view
