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

}  // namespace lilia::model
