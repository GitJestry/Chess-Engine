#pragma once

#include <string>

#include "../constants.hpp"
#include "move_generator.hpp"
#include "position.hpp"
#include "tt4.hpp"
#include "zobrist.hpp"

namespace lilia::model {

class ChessGame {
 public:
  ChessGame() = default;

  void setPosition(const std::string& fen);
  void doMove(core::Square from, core::Square to,
              core::PieceType promotion = core::PieceType::None);
  bb::Piece getPiece(core::Square sq);
  const GameState& getGameState();
  std::vector<Move> generateLegalMoves();
  const Move& getMove(core::Square from, core::Square to);
  bool isKingInCheck(core::Color from) const;
  core::Square getRookSquareFromCastleside(CastleSide castleSide);
  core::Square getKingSquare(core::Color color);
  core::GameResult getResult();

  void checkGameResult();

 private:
  MoveGenerator m_move_gen;
  Position m_position;
  core::GameResult m_result;
};

}  // namespace lilia::model
