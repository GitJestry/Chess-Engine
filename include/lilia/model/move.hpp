#pragma once
#include "model_types.hpp"

namespace lilia {

struct Move {
  core::Square from;
  core::Square to;
  core::PieceType promotion = core::PieceType::None;
  bool isCapture = false;
  bool isCastling = false;
  bool isEnPassant = false;
};

}  // namespace lilia
