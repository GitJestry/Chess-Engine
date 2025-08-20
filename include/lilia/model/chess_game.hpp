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
  void doMove(core::Square from, core::Square to);
  bb::Piece getPiece(core::Square sq);
  const GameState& getGameState();
  const std::vector<Move>& generateLegalMoves();

 private:
  MoveGenerator m_move_gen;
  std::array<std::vector<Move>, 2> m_legal_moves;
  Position m_position;
  TT4 m_tt4{32};
  Zobrist m_zobrist;
};

}  // namespace lilia::model
