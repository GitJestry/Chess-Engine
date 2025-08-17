#pragma once

#include <string>
#include <unordered_map>

#include "../chess_types.hpp"
#include "../core_types.hpp"
#include "board_view.hpp"
#include "piece.hpp"

namespace lilia {

// forward decleration
class ChessAnimator;

class PieceManager {
 public:
  PieceManager(const BoardView& boardRef);

  void initFromFen(std::string& fen);

  [[nodiscard]] Entity::ID_type getPieceID(core::Square pos) const;

  void movePiece(core::Square from, core::Square to);
  void removeAll();

  [[nodiscard]] bool hasPieceOnSquare(core::Square pos) const;
  void setPieceToSquareScreenPos(core::Square from, core::Square to);
  void setPieceToScreenPos(core::Square pos, core::MousePos mousePos);
  void setPieceToScreenPos(core::Square pos, Entity::Position entityPos);

  void renderPieces(sf::RenderWindow& window, const ChessAnimator& chessAnimRef);
  void renderPiece(core::Square pos, sf::RenderWindow& window);

 private:
  void addPiece(core::PieceType type, core::PieceColor color, core::Square pos);
  void removePiece(core::Square pos);

  const BoardView& m_board_view_ref;
  std::unordered_map<core::Square, Piece> m_pieces;
};

}  // namespace lilia
