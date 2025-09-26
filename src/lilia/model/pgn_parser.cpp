#include "lilia/model/pgn_parser.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "lilia/constants.hpp"
#include "lilia/model/chess_game.hpp"
#include "lilia/model/fen_validator.hpp"
#include "lilia/model/move.hpp"
#include "lilia/model/position.hpp"
#include "lilia/uci/uci_helper.hpp"

namespace lilia::model {

namespace {

bool equalsIgnoreCase(const std::string &a, const std::string &b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

bool isResultToken(const std::string &token) {
  return token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*";
}

core::PieceType promotionFromChar(char c) {
  switch (std::tolower(static_cast<unsigned char>(c))) {
    case 'q':
      return core::PieceType::Queen;
    case 'r':
      return core::PieceType::Rook;
    case 'b':
      return core::PieceType::Bishop;
    case 'n':
      return core::PieceType::Knight;
    default:
      return core::PieceType::None;
  }
}

core::Square squareFromStr(const std::string &sq) {
  if (sq.size() != 2) return core::NO_SQUARE;
  char file = static_cast<char>(std::tolower(static_cast<unsigned char>(sq[0])));
  char rank = sq[1];
  if (file < 'a' || file > 'h' || rank < '1' || rank > '8') return core::NO_SQUARE;
  const uint8_t f = static_cast<uint8_t>(file - 'a');
  const uint8_t r = static_cast<uint8_t>(rank - '1');
  return static_cast<core::Square>(f + r * 8);
}

std::string cleanToken(std::string token) {
  // Remove trailing annotations (+, #, !, ?)
  while (!token.empty()) {
    char c = token.back();
    if (c == '+' || c == '#' || c == '!' || c == '?') {
      token.pop_back();
    } else {
      break;
    }
  }
  // Remove "e.p." suffix if present
  const std::string ep1 = "e.p.";
  if (token.size() >= ep1.size()) {
    std::string tail = token.substr(token.size() - ep1.size());
    std::transform(tail.begin(), tail.end(), tail.begin(), [](unsigned char ch) {
      return static_cast<char>(std::tolower(ch));
    });
    if (tail == ep1) {
      token.erase(token.size() - ep1.size());
    }
  }
  return token;
}

bool parseSanMove(const std::string &rawToken, ChessGame &game, model::Move &outMove) {
  std::string token = cleanToken(rawToken);
  if (token.empty()) return false;

  std::string upper = token;
  std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });

  const auto &legal = game.generateLegalMoves();
  auto &board = game.getPositionRefForBot().getBoard();

  if (upper == "O-O" || upper == "0-0") {
    for (const auto &mv : legal) {
      if (mv.castle() == model::CastleSide::KingSide) {
        outMove = mv;
        return true;
      }
    }
    return false;
  }
  if (upper == "O-O-O" || upper == "0-0-0") {
    for (const auto &mv : legal) {
      if (mv.castle() == model::CastleSide::QueenSide) {
        outMove = mv;
        return true;
      }
    }
    return false;
  }

  core::PieceType promotion = core::PieceType::None;
  std::size_t eqPos = token.find('=');
  if (eqPos != std::string::npos) {
    if (eqPos + 1 >= token.size()) return false;
    promotion = promotionFromChar(token[eqPos + 1]);
    if (promotion == core::PieceType::None) return false;
    token.erase(eqPos);  // remove "=Q" etc.
  }

  bool capture = token.find('x') != std::string::npos;
  token.erase(std::remove(token.begin(), token.end(), 'x'), token.end());

  if (token.size() < 2) return false;
  std::string targetStr = token.substr(token.size() - 2);
  core::Square target = squareFromStr(targetStr);
  if (target == core::NO_SQUARE) return false;

  token.erase(token.size() - 2);

  core::PieceType piece = core::PieceType::Pawn;
  std::size_t pos = 0;
  if (!token.empty() && std::isupper(static_cast<unsigned char>(token[0])) &&
      token[0] != 'O') {
    switch (token[0]) {
      case 'K':
        piece = core::PieceType::King;
        break;
      case 'Q':
        piece = core::PieceType::Queen;
        break;
      case 'R':
        piece = core::PieceType::Rook;
        break;
      case 'B':
        piece = core::PieceType::Bishop;
        break;
      case 'N':
        piece = core::PieceType::Knight;
        break;
      default:
        return false;
    }
    pos = 1;
  }

  std::optional<int> fileHint;
  std::optional<int> rankHint;
  for (std::size_t i = pos; i < token.size(); ++i) {
    char c = token[i];
    if (c >= 'a' && c <= 'h') {
      fileHint = c - 'a';
    } else if (c >= '1' && c <= '8') {
      rankHint = c - '1';
    } else {
      return false;
    }
  }

  for (const auto &mv : legal) {
    if (mv.to() != target) continue;
    auto pieceOpt = board.getPiece(mv.from());
    if (!pieceOpt) continue;
    if (pieceOpt->type != piece) continue;
    if (promotion != mv.promotion()) continue;
    if (fileHint && model::bb::file_of(mv.from()) != *fileHint) continue;
    if (rankHint && model::bb::rank_of(mv.from()) != *rankHint) continue;
    if (piece == core::PieceType::Pawn) {
      // Pawn SAN encodes originating file on captures
      if (capture && fileHint && model::bb::file_of(mv.from()) != *fileHint) continue;
      if (!capture && mv.isCapture()) continue;
    } else {
      if (capture != mv.isCapture()) continue;
    }
    outMove = mv;
    return true;
  }
  return false;
}

std::string stripCommentsAndVariations(const std::string &text) {
  std::string cleaned;
  cleaned.reserve(text.size());
  int variationDepth = 0;
  bool inBrace = false;
  bool inSemicolonComment = false;

  for (char ch : text) {
    if (inSemicolonComment) {
      if (ch == '\n' || ch == '\r') {
        inSemicolonComment = false;
        cleaned.push_back(' ');
      }
      continue;
    }
    if (inBrace) {
      if (ch == '}') inBrace = false;
      continue;
    }
    if (ch == ';') {
      inSemicolonComment = true;
      continue;
    }
    if (ch == '{') {
      inBrace = true;
      continue;
    }
    if (ch == '(') {
      ++variationDepth;
      continue;
    }
    if (ch == ')') {
      if (variationDepth > 0) --variationDepth;
      continue;
    }
    if (variationDepth > 0) continue;

    if (std::isspace(static_cast<unsigned char>(ch))) {
      if (!cleaned.empty() && cleaned.back() != ' ') cleaned.push_back(' ');
    } else {
      cleaned.push_back(ch);
    }
  }
  return cleaned;
}

std::unordered_map<std::string, std::string> parseTags(const std::string &pgn) {
  std::unordered_map<std::string, std::string> tags;
  std::istringstream iss(pgn);
  std::string line;
  while (std::getline(iss, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) continue;
    if (line.front() != '[') continue;
    auto close = line.find(']');
    if (close == std::string::npos) continue;
    std::string inner = line.substr(1, close - 1);
    auto space = inner.find(' ');
    if (space == std::string::npos) continue;
    std::string key = inner.substr(0, space);
    std::string value = inner.substr(space + 1);
    if (!value.empty() && value.front() == '"') value.erase(value.begin());
    if (!value.empty() && value.back() == '"') value.pop_back();
    if (!key.empty()) tags[key] = value;
  }
  return tags;
}

std::string extractMovesSection(const std::string &pgn) {
  std::ostringstream oss;
  std::istringstream iss(pgn);
  std::string line;
  while (std::getline(iss, line)) {
    if (!line.empty() && line.front() == '[') continue;
    oss << line << '\n';
  }
  return oss.str();
}

}  // namespace

bool parsePgn(const std::string &pgnText, PgnImport &outImport, std::string *error) {
  outImport = {};
  outImport.startFen = core::START_FEN;
  outImport.finalFen = core::START_FEN;
  outImport.movesUci.clear();
  outImport.termination.clear();

  auto tags = parseTags(pgnText);
  if (auto itFen = tags.find("FEN"); itFen != tags.end()) {
    if (!isFenWellFormed(itFen->second)) {
      if (error) *error = "PGN FEN tag is invalid.";
      return false;
    }
    outImport.startFen = itFen->second;
  }
  if (auto itSetup = tags.find("SetUp"); itSetup != tags.end()) {
    if (equalsIgnoreCase(itSetup->second, "0") && tags.contains("FEN")) {
      // Explicitly indicate standard start despite FEN tag
      outImport.startFen = core::START_FEN;
    }
  }

  std::string movesSection = extractMovesSection(pgnText);
  std::string stripped = stripCommentsAndVariations(movesSection);

  std::istringstream tokens(stripped);
  std::string token;

  ChessGame game;
  game.setPosition(outImport.startFen);

  // Keep track of current FEN for final state
  outImport.finalFen = game.getFen();

  while (tokens >> token) {
    if (token.empty()) continue;
    if (token.find('.') != std::string::npos) {
      // skip move numbers like "1." or "23..."
      continue;
    }
    if (isResultToken(token)) {
      outImport.termination = token;
      break;
    }

    model::Move mv;
    if (!parseSanMove(token, game, mv)) {
      if (error) {
        *error = "Failed to parse SAN token: " + token;
      }
      return false;
    }
    outImport.movesUci.push_back(move_to_uci(mv));
    game.doMove(mv.from(), mv.to(), mv.promotion());
    outImport.finalFen = game.getFen();
  }

  if (outImport.movesUci.empty()) {
    if (error) *error = "PGN contains no moves.";
    return false;
  }

  return true;
}

}  // namespace lilia::model
