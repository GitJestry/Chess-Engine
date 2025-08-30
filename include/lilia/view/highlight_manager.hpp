#pragma once
#include <unordered_map>

#include "../chess_types.hpp"
#include "board_view.hpp"
#include "entity.hpp"

namespace lilia::view {

class HighlightManager {
 public:
  HighlightManager(const BoardView& boardRef);

  void highlightSquare(core::Square pos);
  void highlightAttackSquare(core::Square pos);
  void highlightCaptureSquare(core::Square pos);
  void highlightHoverSquare(core::Square pos);
  void clearAllHighlights();
  void clearHighlightSquare(core::Square pos);
  void clearHighlightHoverSquare(core::Square pos);

  void renderAttack(sf::RenderWindow& window);
    void renderHover(sf::RenderWindow& window);
    void renderSelect(sf::RenderWindow& window);

    void resize();

 private:
  void renderEntitiesToBoard(std::unordered_map<core::Square, Entity>& map,
                             sf::RenderWindow& window);

  const BoardView& m_board_view_ref;

  std::unordered_map<core::Square, Entity> m_hl_attack_squares;
  std::unordered_map<core::Square, Entity> m_hl_select_squares;
  std::unordered_map<core::Square, Entity> m_hl_hover_squares;
};

}  // namespace lilia::view
