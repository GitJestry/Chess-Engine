#pragma once
#include <string>

namespace lilia::model::magic {

/**
 * @brief Serialize current masks (implicit), magics & attack tables to a C++ header file.
 *
 * Precondition: call init_magics() first so tables/magics are built (either generated at
 * runtime or loaded from an existing constants header).
 *
 * The generated header defines:
 *   #define LILIA_MAGIC_HAVE_CONSTANTS 1
 * in namespace:
 *   lilia::model::magic::constants
 *
 * Arrays emitted (prefix defaults to "s"):
 *   prefix_rook_magic, prefix_bishop_magic (arrays of MagicEntry { bb::Bitboard magic; uint8_t shift;
 * }) prefix_rook_table, prefix_bishop_table (arrays of std::vector<bb::Bitboard>)
 *
 * @param outPath path to write header (e.g. "magic_constants.hpp")
 * @param arrayNamePrefix optional prefix used for generated symbol names
 * @return true on success, false on failure (e.g. cannot open file)
 */
bool serialize_magics_to_header(const std::string& outPath,
                                const std::string& arrayNamePrefix = "s");

}  // namespace lilia::model::magic
