#pragma once
#include <vector>

#include "board.hpp"
#include "model_types.hpp"
#include "move.hpp"

namespace lilia {

class MoveGenerator {
 public:
  std::vector<Move> generateMoves(const Board& board, core::Color color);

 private:
  core::Bitboard pawnAttacks(core::Color color, core::Bitboard pawns) const;
  core::Bitboard knightAttacks(core::Bitboard knights) const;
  core::Bitboard bishopAttacks(core::Bitboard bishops, core::Bitboard allPieces) const;
  core::Bitboard rookAttacks(core::Bitboard rooks, core::Bitboard allPieces) const;
  core::Bitboard queenAttacks(core::Bitboard queens, core::Bitboard allPieces) const;
  core::Bitboard kingAttacks(core::Bitboard kings) const;
};

}  // namespace lilia
