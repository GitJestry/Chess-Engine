#include "lilia/model/chess_game.hpp"

#include <sstream>

namespace lilia::model {

// START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
// Square = e3
core::Square stringToSquare(const std::string& strSquare) {
  if (strSquare.size() < 2) return core::NO_SQUARE;
  char f = strSquare[0];
  char r = strSquare[1];
  if (f < 'a' || f > 'h' || r < '1' || r > '8') return core::NO_SQUARE;
  uint8_t file = static_cast<uint8_t>(f - 'a');  // 0..7
  uint8_t rank = static_cast<uint8_t>(r - '1');  // 0..7  <-- important: rank-1
  return static_cast<core::Square>(file + rank * 8);
}

void ChessGame::setPosition(const std::string& fen) {
  std::istringstream iss(fen);
  std::string board, activeColor, castling, enPassant, halfmoveClock, fullmoveNumber;

  iss >> board >> activeColor >> castling >> enPassant >> halfmoveClock >> fullmoveNumber;

  // board
  uint8_t rank = 7;
  uint8_t file = 0;
  for (char ch : board) {
    if (ch == '/') {
      // Nächste Reihe
      rank--;
      file = 0;
    } else if (std::isdigit(ch)) {
      // So viele leere Felder überspringen
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
      m_position.board().setPiece(
          sq, {type, (std::isupper(ch) ? core::Color::White : core::Color::Black)});

      file++;
    }
  }

  // active Color
  m_position.state().sideToMove = (activeColor == "w" ? core::Color::White : core::Color::Black);

  // castling
  uint8_t rights = 0;
  if (castling.find('K') != std::string::npos) rights |= bb::Castling::WK;
  if (castling.find('Q') != std::string::npos) rights |= bb::Castling::WQ;
  if (castling.find('k') != std::string::npos) rights |= bb::Castling::BK;
  if (castling.find('q') != std::string::npos) rights |= bb::Castling::BQ;

  m_position.state().castlingRights = rights;

  // enpassent
  if (enPassant == "-") {
    m_position.state().enPassantSquare = core::NO_SQUARE;  // oder core::NO_SQUARE falls definiert
  } else {
    m_position.state().enPassantSquare = stringToSquare(enPassant);
  }

  // halfmove clock
  m_position.state().halfmoveClock = std::stoi(halfmoveClock);

  // fullmove number
  m_position.state().fullmoveNumber = std::stoi(fullmoveNumber);

  m_position.buildHash();
}

const std::vector<Move>& ChessGame::generateLegalMoves() {
  int side = bb::ci(m_position.state().sideToMove);
  auto moves =
      std::move(m_move_gen.generatePseudoLegalMoves(m_position.board(), m_position.state()));
  // existing code that created 'moves'
  m_legal_moves[side].clear();
  m_legal_moves[side].reserve(moves.size());

  // Filter to legal and push_back, but push ttBest first
  for (const auto& m : moves) {
    if (m_position.doMove(m)) {
      m_legal_moves[side].push_back(m);
      m_position.undoMove();
    }
  }
  return m_legal_moves[side];
}

const GameState& ChessGame::getGameState() {
  return m_position.state();
}

bb::Piece ChessGame::getPiece(core::Square sq) {
  bb::Piece none;
  if (m_position.board().getPiece(sq).has_value()) return m_position.board().getPiece(sq).value();
  return none;
}

void ChessGame::doMove(core::Square from, core::Square to) {
  for (auto m : m_legal_moves[bb::ci(m_position.state().sideToMove)])
    if (m.from == from && m.to == to) m_position.doMove(m);
}

}  // namespace lilia::model
