#include "lilia/model/fen_validator.hpp"

#include <cctype>
#include <sstream>
#include <string>

namespace lilia::model {

namespace {

bool isValidBoard(const std::string &boardField) {
  int rankCount = 0;
  std::size_t i = 0;
  while (i < boardField.size()) {
    int fileSum = 0;
    while (i < boardField.size() && boardField[i] != '/') {
      char c = boardField[i++];
      if (std::isdigit(static_cast<unsigned char>(c))) {
        int n = c - '0';
        if (n <= 0 || n > 8) return false;
        fileSum += n;
      } else {
        switch (c) {
          case 'p':
          case 'r':
          case 'n':
          case 'b':
          case 'q':
          case 'k':
          case 'P':
          case 'R':
          case 'N':
          case 'B':
          case 'Q':
          case 'K':
            ++fileSum;
            break;
          default:
            return false;
        }
      }
      if (fileSum > 8) return false;
    }
    if (fileSum != 8) return false;
    ++rankCount;
    if (i < boardField.size() && boardField[i] == '/') ++i;
  }
  return rankCount == 8;
}

bool isCastlingFieldValid(const std::string &field) {
  if (field == "-") return true;
  for (char c : field) {
    if (c != 'K' && c != 'Q' && c != 'k' && c != 'q') return false;
  }
  return true;
}

bool isEnPassantFieldValid(const std::string &field) {
  if (field == "-") return true;
  if (field.size() != 2) return false;
  char file = field[0];
  char rank = field[1];
  if (file < 'a' || file > 'h') return false;
  return rank == '3' || rank == '6';
}

bool isNonNegativeInteger(const std::string &field) {
  if (field.empty()) return false;
  for (char c : field) {
    if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

}  // namespace

bool isFenWellFormed(const std::string &fen) {
  std::istringstream ss(fen);
  std::string fields[6];
  for (int i = 0; i < 6; ++i) {
    if (!(ss >> fields[i])) return false;
  }
  std::string extra;
  if (ss >> extra) return false;

  if (!isValidBoard(fields[0])) return false;

  if (!(fields[1] == "w" || fields[1] == "b")) return false;

  if (!isCastlingFieldValid(fields[2])) return false;

  if (!isEnPassantFieldValid(fields[3])) return false;

  if (!isNonNegativeInteger(fields[4])) return false;
  if (!isNonNegativeInteger(fields[5])) return false;

  return true;
}

}  // namespace lilia::model
