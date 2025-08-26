#pragma once
#include <vector>

#include "board.hpp"
#include "core/bitboard.hpp"
#include "game_state.hpp"
#include "move.hpp"

namespace lilia::model {

class MoveGenerator {
 public:
  
  void generatePseudoLegalMoves(const Board& b, const GameState& st,
                                std::vector<model::Move>& out) const;

  void genPawnMoves(const Board&, const GameState&, core::Color, std::vector<Move>&) const;
  void genKnightMoves(const Board&, core::Color, std::vector<Move>&) const;
  void genBishopMoves(const Board&, core::Color, std::vector<Move>&) const;
  void genRookMoves(const Board&, core::Color, std::vector<Move>&) const;
  void genQueenMoves(const Board&, core::Color, std::vector<Move>&) const;
  void genKingMoves(const Board&, const GameState&, core::Color, std::vector<Move>&) const;
};

}  
