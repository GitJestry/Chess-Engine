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

  Board& board() { return m_board; }
  const Board& board() const { return m_board; }
  GameState& state() { return m_state; }
  const GameState& state() const { return m_state; }

  // After you’ve placed pieces and set state, call buildHash()
  void buildHash() { m_hash = Zobrist::compute(*this); }
  bb::Bitboard hash() const { return m_hash; }

  // Make/undo a pseudo-legal move; returns false if illegal (king in check)
  bool doMove(const Move& m);
  void undoMove();

  // Attack detection
  bool isSquareAttacked(core::Square sq, core::Color by) const;

 private:
  Board m_board;
  GameState m_state;
  std::vector<StateInfo> m_history;
  bb::Bitboard m_hash = 0;

  // do/undo details
  void applyMove(const Move& m, StateInfo& st);
  void unapplyMove(const StateInfo& st);
  void updateCastlingRightsOnMove(core::Square from, core::Square to);

  // --- Zobrist incremental helpers ---
  inline void hashXorPiece(core::Color c, core::PieceType pt, core::Square s) {
    m_hash ^= Zobrist::piece[bb::ci(c)][static_cast<int>(pt)][s];
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
