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
  void generateNonCapturePromotions(const Board& b, const GameState& st,
                                    std::vector<model::Move>& out) const;

  // Return: Anzahl generierter Züge
  int generatePseudoLegalMoves(const Board&, const GameState&, engine::MoveBuffer& buf);
  int generateCapturesOnly(const Board&, const GameState&, engine::MoveBuffer& buf);
  int generateEvasions(const Board&, const GameState&, engine::MoveBuffer& buf);
  int generateNonCapturePromotions(const Board& b, const GameState& st, engine::MoveBuffer& buf);
};

}  // namespace lilia::model
