#include "visuals/piece_utils.hpp"

// WHITE=0, BLACK=1
// PAWN =0, ROOK=1, BISHOP=2, KNIGHT=3, QUEEN=4, KING=5

namespace utils {
std::string pieceFilename(PieceType type, PieceColor color) {
  const int num_piece_types = 6;
  int piece_num = type + color * num_piece_types;

  // You can use piece_num to generate filename or do whatever you want
  // Example: "piece_0.png", "piece_1.png", etc.
  return "piece_" + std::to_string(piece_num) + ".png";
}
}  // namespace utils
