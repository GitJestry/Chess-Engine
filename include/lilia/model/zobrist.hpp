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
  // ONLY CALL ONCE AT START OF PROGRAM
  static void init(std::uint64_t seed);
  static void init();  // overload ohne Argument

  // Nur hashen, wenn ein EP-Schlag tatsächlich möglich ist
  static inline bb::Bitboard epHashIfRelevant(const Board& b, const GameState& st) noexcept {
    if (st.enPassantSquare == core::NO_SQUARE) return 0;
    const int ep = static_cast<int>(st.enPassantSquare);
    const int file = ep & 7;

    const auto stm = st.sideToMove;
    const int ci = bb::ci(stm);
    const bb::Bitboard pawnsSTM = b.getPieces(stm, core::PieceType::Pawn);
    if (!pawnsSTM) return 0;

    // gibt es einen Bauer der Seite am Zug, der das EP-Feld schlagen könnte?
    if (pawnsSTM & epCaptureMask[ci][ep]) return epFile[file];
    return 0;
  }

  // Vollständiger Hash (teuer, nur für Init/Checks)
  template <class PositionLike>
  static bb::Bitboard compute(const PositionLike& pos) noexcept {
    bb::Bitboard h = 0;

    static constexpr core::PieceType PTs[6] = {core::PieceType::Pawn,   core::PieceType::Knight,
                                               core::PieceType::Bishop, core::PieceType::Rook,
                                               core::PieceType::Queen,  core::PieceType::King};

    const Board& b = pos.getBoard();

    for (int c = 0; c < 2; ++c) {
      const auto color = static_cast<core::Color>(c);
      for (int i = 0; i < 6; ++i) {
        const auto pt = PTs[i];
        const int pti = static_cast<int>(pt);
        bb::Bitboard bbp = b.getPieces(color, pt);
        while (bbp) {
          core::Square s = bb::pop_lsb(bbp);
          h ^= piece[c][pti][s];
        }
      }
    }

    const GameState& st = pos.getState();
    h ^= castling[st.castlingRights & 0xF];
    h ^= epHashIfRelevant(b, st);
    if (st.sideToMove == core::Color::Black) h ^= side;

    return h;
  }

  static bb::Bitboard computePawnKey(const Board& b) noexcept {
    bb::Bitboard h = 0;
    const int pawnIdx = static_cast<int>(core::PieceType::Pawn);

    for (int c = 0; c < 2; ++c) {
      const auto color = static_cast<core::Color>(c);
      bb::Bitboard pawns = b.getPieces(color, core::PieceType::Pawn);
      while (pawns) {
        core::Square s = bb::pop_lsb(pawns);
        h ^= piece[c][pawnIdx][s];
      }
    }
    return h;
  }
};

}  // namespace lilia::model
