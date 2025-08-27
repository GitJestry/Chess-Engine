#pragma once

#include "config.hpp"
#include "lilia/model/move.hpp"
#include "lilia/model/position.hpp"

namespace lilia::engine {

inline int piece_base_value(core::PieceType pt) {
  return base_value[static_cast<int>(pt)];
}

inline int mvv_lva_score(const model::Position& pos, const model::Move& m) {
  if (!m.isCapture && m.promotion == core::PieceType::None) return 0;

  const auto& b = pos.getBoard();

  // Opfer bestimmen (bei EP immer Bauer)
  core::PieceType victimType = core::PieceType::Pawn;
  if (m.isEnPassant) {
    victimType = core::PieceType::Pawn;
  } else if (auto vp = b.getPiece(m.to)) {
    victimType = vp->type;
  }

  // Angreifer ist die Figur am from-Feld (nicht der Promotyp)
  core::PieceType attackerType = core::PieceType::Pawn;
  if (auto ap = b.getPiece(m.from)) attackerType = ap->type;

  const int vVictim = base_value[static_cast<int>(victimType)];
  const int vAttacker = base_value[static_cast<int>(attackerType)];

  // Klassisches MVV/LVA als Kernsignal
  int score = (vVictim << 5) - vAttacker;  // *32 – etwas stärkere Spreizung

  // Promotions: klar staffeln (nur kleines Signal; Hauptboost gibst du in der Hauptsuche)
  if (m.promotion != core::PieceType::None) {
    // Pawn=0,N=1,B=2,R=3,Q=4,King=5, None ggf.=6
    static const int promo_order[7] = {0, 40, 40, 60, 120, 0, 0};  // =Q weit vorne
    score += promo_order[static_cast<int>(m.promotion)];
  }

  if (m.isEnPassant) score += 5;

  // SEE-Signal (optional): Gewinner-Captures nach vorne; Verlierer nach hinten
  // Wenn du eine CP-Variante hast, ersetze den Block durch den auskommentierten CP-Teil.
  if (m.isCapture) {
    // Bool-SEE:
    if (pos.see(m)) {
      score += 500;  // klarer Bonus für „gewinnt Material“
    } else {
      score -= 500;  // stark nach hinten schieben
    }

    // CP-SEE (falls vorhanden):
    // const int seeCp = pos.seeCp(m);  // z. B. +100 = Bauer gewinnt, -900 = Dame verliert
    // score += std::clamp(seeCp, -900, 900); // nur als Tiebreaker addieren
  }

  return score;
}

}  // namespace lilia::engine
