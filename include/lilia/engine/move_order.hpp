#pragma once

#include <vector>

#include "config.hpp"
#include "lilia/model/move.hpp"
#include "lilia/model/position.hpp"

namespace lilia::engine {

// Feinjustierte MVV-LVA-Implementierung
// - realistischere Stückwerte (centipawns)
// - bei Promotion-Captures wird der Angreiferwert als das beförderte Stück gerechnet
// - en-passant (Zielquadrat leer) behandelt Opfer als Bauer
// - primär: hoher Opferwert, sekundär: niedriger Angreiferwert

inline int piece_base_value(core::PieceType pt) {
  return base_value[static_cast<int>(pt)];
}

inline int mvv_lva_score(const model::Position& pos, const model::Move& m) {
  // only meaningful for captures
  if (!m.isCapture) return 0;

  const model::Board& b = pos.board();

  // Determine victim type. For en-passant captures the target square is empty,
  // so we fall back to Pawn which is correct for ep.
  core::PieceType victimType = core::PieceType::Pawn;
  if (auto vp = b.getPiece(m.to)) {
    victimType = vp->type;
  }

  // Determine attacker type. If the move promotes, treat the attacker as the
  // promoted piece (promotion capture), otherwise take the piece from the from-square.
  core::PieceType attackerType = core::PieceType::Pawn;
  if (m.promotion != core::PieceType::None) {
    attackerType = m.promotion;
  } else if (auto ap = b.getPiece(m.from)) {
    attackerType = ap->type;
  }

  const int vVictim = piece_base_value(victimType);
  const int vAttacker = piece_base_value(attackerType);

  // Compose score: prefer capturing valuable pieces (scales primary factor)
  // and prefer using a low-value attacker (secondary).
  // Multiply victim by a factor to make it dominant in sorting while keeping
  // space for attacker-based tiebreak.
  int score = (vVictim * 16) - vAttacker;

  // Small bonus for promotion-captures to ensure they're favored over similar
  // non-promotion captures.
  if (m.promotion != core::PieceType::None) score += 50;

  return score;
}

}  // namespace lilia
