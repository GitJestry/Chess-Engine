#pragma once

#include <string>
#include <vector>

namespace lilia::model {

struct PgnImport {
  std::string startFen;
  std::string finalFen;
  std::vector<std::string> movesUci;
  std::string termination;  // "1-0", "0-1", "1/2-1/2", "*" or empty
};

bool parsePgn(const std::string &pgnText, PgnImport &outImport, std::string *error = nullptr);

}  // namespace lilia::model
