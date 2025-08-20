#pragma once
#include "core/model_types.hpp"

namespace lilia::model {

enum class CastleSide : std::uint8_t { None = 0, KingSide, QueenSide };

struct Move {
  core::Square from = 0;
  core::Square to = 0;
  core::PieceType promotion = core::PieceType::None;
  bool isCapture = false;
  bool isEnPassant = false;
  CastleSide castle = CastleSide::None;
};

constexpr inline bool operator==(const Move& a, const Move& b) {
  return (a.from == b.from && a.to == b.to && a.promotion == b.promotion &&
          a.isCapture == b.isCapture && a.isEnPassant == b.isEnPassant && a.castle == b.castle);
}

}  // namespace lilia::model
