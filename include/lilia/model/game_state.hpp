#pragma once
#include <cstdint>
#include <vector>

#include "core/bitboard.hpp"     // for bb::Bitboard, bb::Castling flags
#include "core/model_types.hpp"  // for core::Color, core::Square, etc.
#include "move.hpp"              // for lilia::model::Move

namespace lilia::model {

struct GameState {
  bb::Bitboard pawnKey = 0;          // incremental pawn hash
  std::uint32_t fullmoveNumber = 1;  // 1..2^32-1
  std::uint16_t halfmoveClock = 0;   // 0..100 is plenty
  std::uint8_t castlingRights =
      bb::Castling::WK | bb::Castling::WQ | bb::Castling::BK | bb::Castling::BQ;
  core::Color sideToMove = core::Color::White;
  core::Square enPassantSquare = core::NO_SQUARE;
};

struct StateInfo {
  Move move{};                        // last move
  bb::Bitboard prevPawnKey{};         // pawn hash before move
  bb::Bitboard zobristKey{};          // full hash before move
  bb::Piece captured{};               // captured piece (type+color)
  std::uint16_t prevHalfmoveClock{};  // halfmove clock before move
  std::uint8_t gaveCheck{0};          // 0/1
  std::uint8_t prevCastlingRights{};  // castling rights before move
  core::Square prevEnPassantSquare{core::NO_SQUARE};
};

struct NullState {
  bb::Bitboard zobristKey{0};  // full hash before null move
  std::uint16_t prevHalfmoveClock{0};
  std::uint32_t prevFullmoveNumber{1};
  std::uint8_t prevCastlingRights{0};
  core::Square prevEnPassantSquare{core::NO_SQUARE};
};

// Sanity checks (cheap, catch accidental changes early)
static_assert(sizeof(GameState) >= 16, "GameState too small?");
static_assert((bb::Castling::WK | bb::Castling::WQ | bb::Castling::BK | bb::Castling::BQ) <= 0xF,
              "Castling rights must fit in 4 bits");
static_assert(std::is_trivially_copyable_v<StateInfo>, "StateInfo should be POD");
static_assert(std::is_trivially_copyable_v<NullState>, "NullState should be POD");

}  // namespace lilia::model
