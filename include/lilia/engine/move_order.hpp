#pragma once

#include <vector>

#include "config.hpp"
#include "lilia/model/move.hpp"
#include "lilia/model/position.hpp"

namespace lilia::engine {

inline int piece_base_value(core::PieceType pt) {
  return base_value[static_cast<int>(pt)];
}

inline int mvv_lva_score(const model::Position& pos, const model::Move& m) {
  // only meaningful for captures
  if (!m.isCapture) return 0;

  const model::Board& b = pos.getBoard();

  core::PieceType victimType = core::PieceType::Pawn;
  if (auto vp = b.getPiece(m.to)) {
    victimType = vp->type;
  }

  core::PieceType attackerType = core::PieceType::Pawn;
  if (m.promotion != core::PieceType::None) {
    attackerType = m.promotion;
  } else if (auto ap = b.getPiece(m.from)) {
    attackerType = ap->type;
  }

  const int vVictim = piece_base_value(victimType);
  const int vAttacker = piece_base_value(attackerType);
  int score = (vVictim * 16) - vAttacker;

  if (m.promotion != core::PieceType::None) score += 50;

  return score;
}

}  // namespace lilia::engine
