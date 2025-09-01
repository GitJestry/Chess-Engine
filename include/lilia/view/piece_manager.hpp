#pragma once

#include <string>
#include <unordered_map>

#include "../controller/mousepos.hpp"
#include "board_view.hpp"
#include "piece.hpp"

namespace lilia::view {

namespace animation {
class ChessAnimator;
}

class PieceManager {
 public:
  PieceManager(const BoardView& boardRef);

  void initFromFen(const std::string& fen);

  [[nodiscard]] Entity::ID_type getPieceID(core::Square pos) const;
  [[nodiscard]] bool isSameColor(core::Square sq1, core::Square sq2) const;

  void movePiece(core::Square from, core::Square to, core::PieceType promotion);
  void addPiece(core::PieceType type, core::Color color, core::Square pos);
  void removePiece(core::Square pos);
  void removeAll();

  [[nodiscard]] core::PieceType getPieceType(core::Square pos) const;
  [[nodiscard]] core::Color getPieceColor(core::Square pos) const;
  [[nodiscard]] bool hasPieceOnSquare(core::Square pos) const;
  [[nodiscard]] Entity::Position getPieceSize(core::Square pos) const;
  void setPieceToSquareScreenPos(core::Square from, core::Square to);
  void setPieceToScreenPos(core::Square pos, core::MousePos mousePos);
  void setPieceToScreenPos(core::Square pos, Entity::Position entityPos);

  void renderPieces(sf::RenderWindow& window, const animation::ChessAnimator& chessAnimRef);
  void renderPiece(core::Square pos, sf::RenderWindow& window);

  // Visual-only helpers for premove previews
  void setPremovePiece(core::Square from, core::Square to);
  void clearPremovePieces();

 private:
  Entity::Position createPiecePositon(core::Square pos);

  const BoardView& m_board_view_ref;
  std::unordered_map<core::Square, Piece> m_pieces;
  // Pieces rendered for premove visualization without affecting board state
  std::unordered_map<core::Square, Piece> m_premove_pieces;
};

}  // namespace lilia::view
