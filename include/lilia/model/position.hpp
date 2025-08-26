#pragma once
#include <cstdint>
#include <vector>

#include "board.hpp"
#include "core/bitboard.hpp"
#include "game_state.hpp"
#include "zobrist.hpp"

namespace lilia::model {

class Position {
 public:
  Position() = default;

  Board& getBoard() { return m_board; }
  const Board& getBoard() const { return m_board; }
  GameState& getState() { return m_state; }
  const GameState& getState() const { return m_state; }

  void buildHash() {
    m_hash = Zobrist::compute(*this);

    bb::Bitboard pk = 0;
    for (core::Square sq = 0; sq < 64; ++sq) {
      auto opt = m_board.getPiece(sq);
      if (!opt.has_value()) continue;
      const bb::Piece p = *opt;
      if (p.type == core::PieceType::Pawn) {
        pk ^= Zobrist::piece[bb::ci(p.color)][static_cast<int>(core::PieceType::Pawn)][sq];
      }
    }
    m_state.pawnKey = pk;
  }
  bb::Bitboard hash() const { return m_hash; }

  bool doMove(const Move& m);
  void undoMove();
  bool doNullMove();
  void undoNullMove();

  bool isSquareAttacked(core::Square sq, core::Color by) const;
  bool checkInsufficientMaterial();
  bool checkMoveRule();
  bool checkRepetition();

  bool inCheck() const;
  bool see(const model::Move& m) const;

 private:
  Board m_board;
  GameState m_state;
  std::vector<StateInfo> m_history;
  bb::Bitboard m_hash = 0;

  struct NullState {
    bb::Bitboard zobristKey;
    std::uint8_t prevCastlingRights;
    core::Square prevEnPassantSquare;
    int prevHalfmoveClock;
    int prevFullmoveNumber;
  };

  std::vector<NullState> m_null_history;

  void applyMove(const Move& m, StateInfo& st);
  void unapplyMove(const StateInfo& st);
  void updateCastlingRightsOnMove(core::Square from, core::Square to);

  void hashXorPiece(core::Color c, core::PieceType pt, core::Square s) {
    m_hash ^= Zobrist::piece[bb::ci(c)][static_cast<int>(pt)][s];

    if (pt == core::PieceType::Pawn) {
      m_state.pawnKey ^= Zobrist::piece[bb::ci(c)][static_cast<int>(core::PieceType::Pawn)][s];
    }
  }

  inline void hashXorSide() { m_hash ^= Zobrist::side; }
  inline void hashSetCastling(std::uint8_t prev, std::uint8_t next) {
    m_hash ^= Zobrist::castling[prev & 0xF];
    m_hash ^= Zobrist::castling[next & 0xF];
  }
  inline void hashClearEP() {
    if (m_state.enPassantSquare != 64) {
      m_hash ^= Zobrist::epFile[bb::file_of(m_state.enPassantSquare)];
    }
  }
  inline void hashSetEP(core::Square epSq) {
    if (epSq != 64) {
      m_hash ^= Zobrist::epFile[bb::file_of(epSq)];
    }
  }
};

}  // namespace lilia::model
