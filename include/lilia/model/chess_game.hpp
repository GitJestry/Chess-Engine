#pragma once

#include <string>

#include "move_generator.hpp"
#include "position.hpp"
#include "tt4.hpp"
#include "zobrist.hpp"

namespace lilia::model {

class ChessGame {
 public:
  ChessGame() = default;

  void setPosition(const std::string& fen);
  void doMove(core::Square from, core::Square to,
              core::PieceType promotion = core::PieceType::None);
  bb::Piece getPiece(core::Square sq);
  const GameState& getGameState();
  const std::vector<Move>& generateLegalMoves();
  const Move& getMove(core::Square from, core::Square to);
  bool isKingInCheck(core::Color from) const;
  core::Square getRookSquareFromCastleside(CastleSide castleSide);

 private:
  MoveGenerator m_move_gen;
  std::array<std::vector<Move>, 2> m_legal_moves;
  Position m_position;
};

}  // namespace lilia::model
