#pragma once

#include <string>

namespace lilia::core {
const std::string START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
enum GameResult { ONGOING, CHECKMATE, REPETITION, MOVERULE, STALEMATE, INSUFFICIENT };
}  // namespace lilia::core
