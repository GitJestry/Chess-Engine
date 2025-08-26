#include "lilia/model/chess_game.hpp"

#include <sstream>
#include <optional>

namespace lilia::model {

core::Square stringToSquare(const std::string& strSquare) {
  if (strSquare.size() < 2) return core::NO_SQUARE;
  char f = strSquare[0];
  char r = strSquare[1];
  if (f < 'a' || f > 'h' || r < '1' || r > '8') return core::NO_SQUARE;
  uint8_t file = static_cast<uint8_t>(f - 'a');  
  uint8_t rank = static_cast<uint8_t>(r - '1');  
  return static_cast<core::Square>(file + rank * 8);
}

inline int squareFromUCI(const std::string& sq) {
  if (sq.size() != 2) return -1;
  int file = sq[0] - 'a';
  int rank = sq[1] - '1';
  return rank * 8 + file;
}

void ChessGame::doMoveUCI(const std::string& uciMove) {
  if (uciMove.size() < 4) return;

  int from = squareFromUCI(uciMove.substr(0, 2));
  int to = squareFromUCI(uciMove.substr(2, 2));
  core::PieceType promo = core::PieceType::None;

  if (uciMove.size() == 5) {  
    char c = uciMove[4];
    switch (c) {
      case 'q':
        promo = core::PieceType::Queen;
        break;
      case 'r':
        promo = core::PieceType::Rook;
        break;
      case 'b':
        promo = core::PieceType::Bishop;
        break;
      case 'n':
        promo = core::PieceType::Knight;
        break;
      default:
        promo = core::PieceType::None;
        break;
    }
  }
  doMove(from, to, promo);
}

std::optional<Move> ChessGame::getMove(core::Square from, core::Square to) {
  int side = bb::ci(m_position.getState().sideToMove);
  const auto& moves = generateLegalMoves();
  for (const auto& m : moves) {
    if (m.from == from && m.to == to) {
      return m;
    }
  }
  return std::nullopt;
}

void ChessGame::setPosition(const std::string& fen) {
  std::istringstream iss(fen);
  std::string board, activeColor, castling, enPassant, halfmoveClock, fullmoveNumber;

  iss >> board >> activeColor >> castling >> enPassant >> halfmoveClock >> fullmoveNumber;

  
  uint8_t rank = 7;
  uint8_t file = 0;
  for (char ch : board) {
    if (ch == '/') {
      
      rank--;
      file = 0;
    } else if (std::isdigit(ch)) {
      
      file += ch - '0';
    } else {
      core::Square sq = file + rank * 8;
      core::PieceType type;
      switch (std::tolower(ch)) {
        case 'k':
          type = core::PieceType::King;
          break;
        case 'p':
          type = core::PieceType::Pawn;
          break;
        case 'n':
          type = core::PieceType::Knight;
          break;
        case 'b':
          type = core::PieceType::Bishop;
          break;
        case 'r':
          type = core::PieceType::Rook;
          break;
        case 'q':
          type = core::PieceType::Queen;
          break;
        default:
          throw std::runtime_error("Ungültiges Zeichen im FEN: " + std::string(1, ch));
      }
      m_position.getBoard().setPiece(
          sq, {type, (std::isupper(ch) ? core::Color::White : core::Color::Black)});

      file++;
    }
  }

  // active Color
  m_position.getState().sideToMove = (activeColor == "w" ? core::Color::White : core::Color::Black);
  
  uint8_t rights = 0;
  if (castling.find('K') != std::string::npos) rights |= bb::Castling::WK;
  if (castling.find('Q') != std::string::npos) rights |= bb::Castling::WQ;
  if (castling.find('k') != std::string::npos) rights |= bb::Castling::BK;
  if (castling.find('q') != std::string::npos) rights |= bb::Castling::BQ;

  m_position.getState().castlingRights = rights;

  if (enPassant == "-") {
    m_position.getState().enPassantSquare = core::NO_SQUARE;  // oder core::NO_SQUARE falls definiert
  } else {
    m_position.getState().enPassantSquare = stringToSquare(enPassant);
  }

  // halfmove clock
  m_position.getState().halfmoveClock = std::stoi(halfmoveClock);

  // fullmove number
  m_position.getState().fullmoveNumber = std::stoi(fullmoveNumber);

  m_position.buildHash();
}

void ChessGame::buildHash() {
  m_position.buildHash();
}

const std::vector<Move>& ChessGame::generateLegalMoves() {
  int side = bb::ci(m_position.getState().sideToMove);
  m_pseudo_moves.clear();
  m_legal_moves.clear();
  m_move_gen.generatePseudoLegalMoves(m_position.getBoard(), m_position.getState(), m_pseudo_moves);
  for (const auto& m : m_pseudo_moves) {
    if (m_position.doMove(m)) {
      m_position.undoMove();
      m_legal_moves.push_back(m);
    }
  }
  return m_legal_moves;
}

const GameState& ChessGame::getGameState() {
  return m_position.getState();
}

core::Square ChessGame::getRookSquareFromCastleside(CastleSide castleSide, core::Color side) {
  if (castleSide == CastleSide::KingSide) {
    if (side == core::Color::White)
      return static_cast<core::Square>(7);
    else
      return static_cast<core::Square>(63);

  } else if (castleSide == CastleSide::QueenSide) {
    if (side == core::Color::White)
      return static_cast<core::Square>(0);
    else
      return static_cast<core::Square>(56);
  }
  return core::NO_SQUARE;
}

core::Square ChessGame::getKingSquare(core::Color color) {
  bb::Bitboard kbb = m_position.getBoard().getPieces(color, core::PieceType::King);
  core::Square ksq = static_cast<core::Square>(bb::ctz64(kbb));
  return ksq;
}

void ChessGame::checkGameResult() {
  if (generateLegalMoves().empty()) {
    if (isKingInCheck(m_position.getState().sideToMove))
      m_result = core::GameResult::CHECKMATE;
    else
      m_result = core::GameResult::STALEMATE;
  }
  if (m_position.checkInsufficientMaterial()) m_result = core::GameResult::INSUFFICIENT;
  if (m_position.checkMoveRule()) m_result = core::GameResult::MOVERULE;
  if (m_position.checkRepetition()) m_result = core::GameResult::REPETITION;
}

core::GameResult ChessGame::getResult() {
  return m_result;
}

bb::Piece ChessGame::getPiece(core::Square sq) {
  bb::Piece none;
  if (m_position.getBoard().getPiece(sq).has_value()) return m_position.getBoard().getPiece(sq).value();
  return none;
}

void ChessGame::doMove(core::Square from, core::Square to, core::PieceType promotion) {
  for (const auto& m : generateLegalMoves())
    if (m.from == from && m.to == to && m.promotion == promotion) m_position.doMove(m);
}

bool ChessGame::isKingInCheck(core::Color from) const {
  bb::Bitboard kbb = m_position.getBoard().getPieces(from, core::PieceType::King);
  core::Square ksq = static_cast<core::Square>(bb::ctz64(kbb));
  return m_position.isSquareAttacked(ksq, ~from);
}

Position& ChessGame::getPositionRefForBot() {
  return m_position;
}
}  
