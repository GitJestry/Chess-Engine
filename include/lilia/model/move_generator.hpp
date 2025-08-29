#pragma once

#include <vector>

#include "lilia/engine/move_buffer.hpp"
#include "lilia/model/board.hpp"
#include "lilia/model/game_state.hpp"

namespace lilia::model {

struct Move;

class MoveGenerator {
 public:
  // Volle Pseudolegal-Gen (Quiet + Captures + Promos + EP + evtl. Castle)
  void generatePseudoLegalMoves(const Board& b, const GameState& st, std::vector<Move>& out) const;

  // Nur Schläge + Promotions (inkl. EP und Quiet-Promos)
  void generateCapturesOnly(const Board& b, const GameState& st, std::vector<Move>& out) const;

  // Evasions bei Schach: sichere Königszüge plus (bei Single-Check) Checker schlagen / blocken
  // Pseudolegal – finale Legalität via doMove()
  void generateEvasions(const Board& b, const GameState& st, std::vector<Move>& out) const;

  // NEU: schnelle Overloads in festen Buffer
  // Return: Anzahl generierter Züge
  int generatePseudoLegalMoves(const Board&, const GameState&, engine::MoveBuffer& buf);
  int generateCapturesOnly(const Board&, const GameState&, engine::MoveBuffer& buf);
  int generateEvasions(const Board&, const GameState&, engine::MoveBuffer& buf);

 private:
  // Standard-Teilgeneration (intern von generatePseudoLegalMoves benutzt)
  void genPawnMoves(const Board& b, const GameState& st, core::Color side,
                    std::vector<Move>& out) const;
  void genKnightMoves(const Board& b, core::Color side, std::vector<Move>& out) const;
  void genBishopMoves(const Board& b, core::Color side, std::vector<Move>& out) const;
  void genRookMoves(const Board& b, core::Color side, std::vector<Move>& out) const;
  void genQueenMoves(const Board& b, core::Color side, std::vector<Move>& out) const;
  void genKingMoves(const Board& b, const GameState& st, core::Color side,
                    std::vector<Move>& out) const;
};

}  // namespace lilia::model
