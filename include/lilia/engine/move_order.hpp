#pragma once

#include "config.hpp"
#include "lilia/model/move.hpp"
#include "lilia/model/position.hpp"

namespace lilia::engine {

inline int mvv_lva_fast(const model::Position& pos, const model::Move& m) {
  if (!m.isCapture() && m.promotion() == core::PieceType::None) return 0;
  const auto& b = pos.getBoard();

  core::PieceType victimType = core::PieceType::Pawn;
  if (m.isEnPassant()) {
    victimType = core::PieceType::Pawn;
  } else {
    auto vp = b.getPiece(m.to());
    if (vp.type != core::PieceType::None) {
      victimType = vp.type;
    }
  }

  core::PieceType attackerType = core::PieceType::Pawn;
  auto ap = b.getPiece(m.from());
  if (ap.type != core::PieceType::None) attackerType = ap.type;

  const int vVictim = base_value[static_cast<int>(victimType)];
  const int vAttacker = base_value[static_cast<int>(attackerType)];

  int score = (vVictim << 5) - vAttacker;  // *32 Spreizung
  if (m.promotion() != core::PieceType::None) {
    static constexpr int promo_order[7] = {0, 40, 40, 60, 120, 0, 0};
    score += promo_order[static_cast<int>(m.promotion())];
  }
  if (m.isEnPassant()) score += 5;
  return score;
}

}  // namespace lilia::engine
