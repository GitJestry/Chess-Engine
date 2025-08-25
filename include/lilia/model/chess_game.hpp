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
  void buildHash();
  void doMove(core::Square from, core::Square to,
              core::PieceType promotion = core::PieceType::None);
  void doMoveUCI(const std::string& uciMove);
  std::string move_to_uci(const model::Move& m);

  bb::Piece getPiece(core::Square sq);
  const GameState& getGameState();
  std::vector<Move> generateLegalMoves();
  const Move& getMove(core::Square from, core::Square to);
  bool isKingInCheck(core::Color from) const;
  core::Square getRookSquareFromCastleside(CastleSide castleSide, core::Color side);
  core::Square getKingSquare(core::Color color);
  core::GameResult getResult();
  Position& getPositionRefForBot();

  void checkGameResult();

 private:
  MoveGenerator m_move_gen;
  Position m_position;
  core::GameResult m_result;
};

}  // namespace lilia::model
