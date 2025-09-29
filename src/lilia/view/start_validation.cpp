#include "lilia/view/start_validation.hpp"

#include <cctype>
#include <sstream>
#include <string>

namespace lilia::view {

namespace {

bool isCastlingFieldValid(const std::string& field) {
  if (field == "-") return true;
  for (char c : field) {
    if (c != 'K' && c != 'Q' && c != 'k' && c != 'q') return false;
  }
  return !field.empty();
}

bool isEnPassantValid(const std::string& field) {
  if (field == "-") return true;
  if (field.size() != 2) return false;
  char file = field[0];
  char rank = field[1];
  if (file < 'a' || file > 'h') return false;
  return rank == '3' || rank == '6';
}

bool isNonNegativeInteger(const std::string& field) {
  if (field.empty()) return false;
  for (unsigned char c : field) {
    if (!std::isdigit(c)) return false;
  }
  return true;
}

std::string trim(const std::string& str) {
  const auto first = str.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  const auto last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, last - first + 1);
}

}  // namespace

bool basicFenCheck(const std::string& fen) {
  std::istringstream ss(fen);
  std::string fields[6];
  for (int i = 0; i < 6; ++i) {
    if (!(ss >> fields[i])) return false;
  }
  std::string extra;
  if (ss >> extra) return false;

  int rankCount = 0;
  std::size_t i = 0;
  while (i < fields[0].size()) {
    int fileSum = 0;
    while (i < fields[0].size() && fields[0][i] != '/') {
      char c = fields[0][i++];
      if (std::isdigit(static_cast<unsigned char>(c))) {
        int n = c - '0';
        if (n <= 0 || n > 8) return false;
        fileSum += n;
      } else {
        if (std::string("prnbqkPRNBQK").find(c) == std::string::npos) return false;
        fileSum += 1;
      }
    }
    if (fileSum != 8) return false;
    if (i < fields[0].size() && fields[0][i] == '/') ++i;
    ++rankCount;
  }
  if (rankCount != 8) return false;

  if (fields[1] != "w" && fields[1] != "b") return false;
  if (!isCastlingFieldValid(fields[2])) return false;
  if (!isEnPassantValid(fields[3])) return false;
  if (!isNonNegativeInteger(fields[4])) return false;
  if (!isNonNegativeInteger(fields[5])) return false;
  if (std::stoi(fields[5]) <= 0) return false;
  return true;
}

bool basicPgnCheck(const std::string& pgn) {
  const std::string trimmed = trim(pgn);
  if (trimmed.empty()) return false;

  bool hasMoveNumber = trimmed.find('.') != std::string::npos;
  bool hasTag = trimmed.find('[') != std::string::npos;
  bool hasResult = trimmed.find("1-0") != std::string::npos ||
                   trimmed.find("0-1") != std::string::npos ||
                   trimmed.find("1/2-1/2") != std::string::npos;

  for (unsigned char c : trimmed) {
    if (c == '\n' || c == '\r' || c == '\t') continue;
    if (c < 32 || c > 126) return false;
  }

  return hasMoveNumber || hasTag || hasResult;
}

}  // namespace lilia::view

