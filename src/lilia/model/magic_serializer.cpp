#include "lilia/model/magic_serializer.hpp"

#include <array>
#include <fstream>
#include <iomanip>
#include <vector>

#include "lilia/model/core/magic.hpp"

namespace lilia::model::magic {

bool serialize_magics_to_header(const std::string& outPath, const std::string& prefix) {
  std::ofstream out(outPath);
  if (!out) return false;

  out << "#pragma once\n\n";
  out << "#include <array>\n";
  out << "#include <vector>\n";
  out << "#include \"lilia/model/core/magic.hpp\"\n\n";
  out << "namespace lilia::model::magic::constants {\n";

  const auto& rmag = rook_magics();
  out << "inline constexpr std::array<Magic, 64> " << prefix
      << "rook_magic = {\n";
  for (int i = 0; i < 64; ++i) {
    const Magic& m = rmag[i];
    out << "    Magic{0x" << std::hex << std::uppercase << m.magic << "ULL, "
        << std::dec << static_cast<int>(m.shift) << "}";
    if (i != 63) out << ",";
    out << "\n";
  }
  out << "};\n\n";

  const auto& bmag = bishop_magics();
  out << "inline constexpr std::array<Magic, 64> " << prefix
      << "bishop_magic = {\n";
  for (int i = 0; i < 64; ++i) {
    const Magic& m = bmag[i];
    out << "    Magic{0x" << std::hex << std::uppercase << m.magic << "ULL, "
        << std::dec << static_cast<int>(m.shift) << "}";
    if (i != 63) out << ",";
    out << "\n";
  }
  out << "};\n\n";

  const auto& rtab = rook_tables();
  out << "inline const std::array<std::vector<bb::Bitboard>, 64> " << prefix
      << "rook_table = {\n";
  for (int i = 0; i < 64; ++i) {
    const auto& vec = rtab[i];
    out << "    std::vector<bb::Bitboard>{";
    for (size_t j = 0; j < vec.size(); ++j) {
      out << "0x" << std::hex << std::uppercase << vec[j] << "ULL";
      if (j + 1 < vec.size()) out << ", ";
    }
    out << "}";
    if (i != 63) out << ",";
    out << "\n";
  }
  out << "};\n\n";

  const auto& btab = bishop_tables();
  out << "inline const std::array<std::vector<bb::Bitboard>, 64> " << prefix
      << "bishop_table = {\n";
  for (int i = 0; i < 64; ++i) {
    const auto& vec = btab[i];
    out << "    std::vector<bb::Bitboard>{";
    for (size_t j = 0; j < vec.size(); ++j) {
      out << "0x" << std::hex << std::uppercase << vec[j] << "ULL";
      if (j + 1 < vec.size()) out << ", ";
    }
    out << "}";
    if (i != 63) out << ",";
    out << "\n";
  }
  out << "};\n\n";

  out << "} // namespace lilia::model::magic::constants\n";

  return true;
}

}  // namespace lilia::model::magic

