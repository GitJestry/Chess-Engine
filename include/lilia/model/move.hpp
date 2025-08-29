#pragma once
#include <cstdint>
#include <type_traits>

#include "core/model_types.hpp"

namespace lilia::model {

enum class CastleSide : std::uint8_t { None = 0, KingSide, QueenSide };

// Typisierte Bitfields -> kompatibel mit enum class Vergleichen/Zuweisungen
struct Move {
  // 6+6+4+1+1+2 = 20 Bits (+ Reserve)
  core::Square from : 6;          // 0..63
  core::Square to : 6;            // 0..63
  core::PieceType promotion : 4;  // 4 Bits, genug für None/N/B/R/Q/... (sicher)
  bool isCapture : 1;
  bool isEnPassant : 1;
  CastleSide castle : 2;
  unsigned _reserved : 12;  // Platz für Flags (SEE, hash, etc.)

  constexpr Move() noexcept
      : from(static_cast<core::Square>(0)),
        to(static_cast<core::Square>(0)),
        promotion(core::PieceType::None),
        isCapture(false),
        isEnPassant(false),
        castle(CastleSide::None),
        _reserved(0) {}

  constexpr Move(core::Square f, core::Square t, core::PieceType promo, bool cap, bool ep,
                 CastleSide cs) noexcept
      : from(f),
        to(t),
        promotion(promo),
        isCapture(cap),
        isEnPassant(ep),
        castle(cs),
        _reserved(0) {}
};

constexpr inline bool operator==(const Move& a, const Move& b) noexcept {
  return (a.from == b.from && a.to == b.to && a.promotion == b.promotion &&
          a.isCapture == b.isCapture && a.isEnPassant == b.isEnPassant && a.castle == b.castle);
}

static_assert(std::is_trivially_copyable_v<Move>, "Move must be trivially copyable");
// Je nach Compiler/ABI können es 4–8 Bytes sein. Wichtig ist: klein & trivially copyable.
static_assert(sizeof(Move) <= 8, "Move should be tightly packed (<= 8 bytes)");

}  // namespace lilia::model
