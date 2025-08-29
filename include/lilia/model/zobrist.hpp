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

  // EP-Angriffsquellen pro Seite & Feld (precomputed)
  // Für White: SW/SE vom Ziel, für Black: NW/NE vom Ziel
  static bb::Bitboard epCaptureMask[2][64];

  // Init: mit Seed oder bequem ohne Argument (fester Seed, deterministic)
  static void init(std::uint64_t seed);
  static void init();  // overload ohne Argument

  // Nur hashen, wenn ein EP-Schlag tatsächlich möglich ist
  static inline bb::Bitboard epHashIfRelevant(const Board& b, const GameState& st) {
    if (st.enPassantSquare == core::NO_SQUARE) return 0;
    const int ep = static_cast<int>(st.enPassantSquare);
    const int file = ep & 7;  // 0..7

    const int ci = bb::ci(st.sideToMove);
    const bb::Bitboard pawns = b.getPieces(st.sideToMove, core::PieceType::Pawn);

    // gibt es einen Bauer der Seite-zu-Zug, der das EP-Feld schlagen könnte?
    if (pawns & epCaptureMask[ci][ep]) return epFile[file];
    return 0;
  }

  // Vollständiger Hash (teuer, nur für Init/Checks)
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

  // Nur-Bauern-Hash (nützlich für Pawn-Table oder Verifikation)
  static bb::Bitboard computePawnKey(const Board& b) {
    bb::Bitboard h = 0;
    for (int c = 0; c < 2; ++c) {
      bb::Bitboard pawns = b.getPieces(static_cast<core::Color>(c), core::PieceType::Pawn);
      while (pawns) {
        core::Square s = static_cast<core::Square>(bb::ctz64(pawns));
        pawns &= pawns - 1;
        h ^= piece[c][/*Pawn*/ 0][s];
      }
    }
    return h;
  }
};

}  // namespace lilia::model
