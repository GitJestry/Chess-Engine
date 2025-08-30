#pragma once
#include <cstdint>
#include <vector>

#include "../engine/eval_acc.hpp"
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

  // Vollständigen Hash und pawnKey aus der aktuellen Stellung neu berechnen
  // header: typsicher & nützlich
  [[nodiscard]] inline std::uint64_t hash() const noexcept {
    return static_cast<std::uint64_t>(m_hash);
  }
  [[nodiscard]] inline bool lastMoveGaveCheck() const noexcept {
    return !m_history.empty() && m_history.back().gaveCheck != 0;
  }

  // buildHash(): schneller & ohne Square-Loop
  void buildHash() {
    // Vollhash (inkl. EP-Relevanz!) – ACHTUNG: Zobrist::compute(*this) muss die gleiche EP-Logik
    // benutzen
    m_hash = Zobrist::compute(*this);

    // pawnKey neu aufbauen
    bb::Bitboard pk = 0;
    for (auto c : {core::Color::White, core::Color::Black}) {
      bb::Bitboard pawns = m_board.getPieces(c, core::PieceType::Pawn);
      while (pawns) {
        core::Square s = bb::pop_lsb(pawns);
        pk ^= Zobrist::piece[bb::ci(c)][static_cast<int>(core::PieceType::Pawn)][s];
      }
    }
    m_state.pawnKey = pk;
  }

  // Make/Unmake
  bool doMove(const Move& m);
  void undoMove();
  bool doNullMove();
  void undoNullMove();

  // Statusabfragen
  bool checkInsufficientMaterial();
  bool checkMoveRule();
  bool checkRepetition();

  bool inCheck() const;
  bool see(const model::Move& m) const;

  const engine::EvalAcc& getEvalAcc() const noexcept { return evalAcc_; }
  void rebuildEvalAcc() { evalAcc_.build_from_board(m_board); }

 private:
  Board m_board;
  GameState m_state;
  std::vector<StateInfo> m_history;
  bb::Bitboard m_hash = 0;
  engine::EvalAcc evalAcc_;
  std::vector<NullState> m_null_history;

  // interne Helfer
  void applyMove(const Move& m, StateInfo& st);
  void unapplyMove(const StateInfo& st);

  // Zobrist/PawnKey inkrementell
  inline void hashXorPiece(core::Color c, core::PieceType pt, core::Square s) {
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

  // EP-Hash nur dann xoren, wenn EP in der aktuellen State-Kombination relevant ist.
  // Wichtig: Vor State-Änderungen aufrufen, um "alt" aus dem Hash zu entfernen,
  // und NACH allen State-Änderungen erneut, um "neu" zu addieren.
  void xorEPRelevant() {
    const auto ep = m_state.enPassantSquare;
    if (ep == core::NO_SQUARE) return;

    const auto stm = m_state.sideToMove;
    const bb::Bitboard pawnsSTM = m_board.getPieces(stm, core::PieceType::Pawn);
    if (!pawnsSTM) return;  // nichts zu tun

    const int epIdx = static_cast<int>(ep);
    const int file = epIdx & 7;
    const int ci = bb::ci(stm);

    if (pawnsSTM & Zobrist::epCaptureMask[ci][epIdx]) m_hash ^= Zobrist::epFile[file];
  }
};

}  // namespace lilia::model
