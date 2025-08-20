#include "lilia/model/zobrist.hpp"

#include "lilia/model/board.hpp"
#include "lilia/model/core/random.hpp"
#include "lilia/model/game_state.hpp"
#include "lilia/model/position.hpp"

namespace lilia::model {

void Zobrist::init(bb::Bitboard seed) {
  random::SplitMix64 rng(seed);

  for (int c = 0; c < 2; ++c)
    for (int t = 0; t < 6; ++t)
      for (int s = 0; s < 64; ++s) piece[c][t][s] = rng.next();

  for (int i = 0; i < 16; ++i) castling[i] = rng.next();
  for (int f = 0; f < 8; ++f) epFile[f] = rng.next();

  side = rng.next();
}

template <class PositionLike>
bb::Bitboard Zobrist::compute(const PositionLike& pos) {
  bb::Bitboard h = 0;

  const Board& b = pos.board();
  for (int c = 0; c < 2; ++c) {
    for (int t = 0; t < 6; ++t) {
      bb::Bitboard bitboard =
          b.pieces(static_cast<core::Color>(c), static_cast<core::PieceType>(t));
      while (bitboard) {
        core::Square s = static_cast<core::Square>(bb::ctz64(bitboard));
        bitboard &= bitboard - 1;
        h ^= piece[c][t][s];
      }
    }
  }

  const GameState& st = pos.state();
  h ^= castling[st.castlingRights & 0xF];

  if (st.enPassantSquare != core::NO_SQUARE) {
    int file = bb::file_of(st.enPassantSquare);
    h ^= epFile[file];
  }

  if (st.sideToMove == core::Color::Black) h ^= side;

  return h;
}

// explicit instantiation for our Position type (defined in position.hpp)
template bb::Bitboard Zobrist::compute<class Position>(const Position&);

}  // namespace lilia::model
