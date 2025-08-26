#include "lilia/model/magic_serializer.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

#include "lilia/model/core/magic.hpp"

namespace lilia::model::magic {

namespace {

std::string uint64_to_hex(uint64_t v) {
  std::ostringstream oss;
  oss << "0x" << std::hex << std::setw(16) << std::setfill('0') << v;
  return oss.str();
}

}  

bool serialize_magics_to_header(const std::string& outPath, const std::string& prefix) {
  const auto& rmag = rook_magics();
  const auto& bmag = bishop_magics();
  const auto& rtab = rook_tables();
  const auto& btab = bishop_tables();

  std::ofstream out(outPath, std::ios::out | std::ios::trunc);
  if (!out.is_open()) return false;

  out << "#pragma once\n\n";
  out << "#include <array>\n#include <vector>\n#include <cstdint>\n\n";
  out << "
  out << "
  out << "#define LILIA_MAGIC_HAVE_CONSTANTS 1\n\n";
  out << "namespace lilia::model::magic::constants {\n\n";

  out << "struct MagicEntry { std::uint64_t magic; std::uint8_t shift; };\n\n";

  
  out << "static inline const std::array<MagicEntry, 64> " << prefix << "_rook_magic = {\n    {";
  for (size_t i = 0; i < 64; ++i) {
    out << "{ " << uint64_to_hex(rmag[i].magic) << ", " << static_cast<unsigned>(rmag[i].shift)
        << " }";
    if (i + 1 < 64) out << ", ";
    if ((i + 1) % 4 == 0) out << "\n    ";
  }
  out << "\n    }\n};\n\n";

  
  out << "static inline const std::array<MagicEntry, 64> " << prefix << "_bishop_magic = {\n    {";
  for (size_t i = 0; i < 64; ++i) {
    out << "{ " << uint64_to_hex(bmag[i].magic) << ", " << static_cast<unsigned>(bmag[i].shift)
        << " }";
    if (i + 1 < 64) out << ", ";
    if ((i + 1) % 4 == 0) out << "\n    ";
  }
  out << "\n    }\n};\n\n";

  
  out << "
  out << "static inline const std::array<std::vector<std::uint64_t>, 64> " << prefix
      << "_rook_table = {\n    {\n";
  for (size_t i = 0; i < 64; ++i) {
    out << "        std::vector<std::uint64_t>{";
    const auto& vec = rtab[i];
    for (size_t j = 0; j < vec.size(); ++j) {
      out << uint64_to_hex(vec[j]);
      if (j + 1 < vec.size()) out << ", ";
    }
    out << "}";
    if (i + 1 < 64)
      out << ",\n";
    else
      out << "\n";
  }
  out << "    }\n};\n\n";

  
  out << "
  out << "static inline const std::array<std::vector<std::uint64_t>, 64> " << prefix
      << "_bishop_table = {\n    {\n";
  for (size_t i = 0; i < 64; ++i) {
    out << "        std::vector<std::uint64_t>{";
    const auto& vec = btab[i];
    for (size_t j = 0; j < vec.size(); ++j) {
      out << uint64_to_hex(vec[j]);
      if (j + 1 < vec.size()) out << ", ";
    }
    out << "}";
    if (i + 1 < 64)
      out << ",\n";
    else
      out << "\n";
  }
  out << "    }\n};\n\n";

  out << "} 
  out.close();
  return true;
}

}  
