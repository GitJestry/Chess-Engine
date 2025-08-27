#pragma once
#include <cstdint>

#include "board.hpp"
#include "core/bitboard.hpp"
#include "game_state.hpp"

namespace lilia::model {

struct Zobrist {
  // Zobrist-Tables
  static bb::Bitboard piece[2][6][64];
  static bb::Bitboard castling[16];
  static bb::Bitboard epFile[8];
  static bb::Bitboard side;

  // Init: mit Seed oder bequem ohne Argument (fester Seed, deterministic)
  static void init(std::uint64_t seed);
  static void init();  // overload ohne Argument

  // Nur hashen, wenn ein EP-Schlag tatsächlich möglich ist
  static bb::Bitboard epHashIfRelevant(const Board& b, const GameState& st);

  // Vollständiger Hash
  template <class PositionLike>
  static bb::Bitboard compute(const PositionLike& pos) {
    bb::Bitboard h = 0;

    const Board& b = pos.getBoard();
    for (int c = 0; c < 2; ++c) {
      for (int t = 0; t < 6; ++t) {
        bb::Bitboard bbp =
            b.getPieces(static_cast<core::Color>(c), static_cast<core::PieceType>(t));
        while (bbp) {
          core::Square s = static_cast<core::Square>(bb::ctz64(bbp));
          bbp &= bbp - 1;
          h ^= piece[c][t][s];
        }
      }
    }

    const GameState& st = pos.getState();
    h ^= castling[st.castlingRights & 0xF];
    h ^= epHashIfRelevant(b, st);

    if (st.sideToMove == core::Color::Black) h ^= side;
    return h;
  }
};

}  // namespace lilia::model
