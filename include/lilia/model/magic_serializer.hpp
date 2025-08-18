#pragma once
#include <string>

namespace lilia::model::magic {

/**
 * @brief Serialize current magics & attack tables to a C++ header file.
 *
 * Precondition: call init_magics() first so tables/magics are built.
 *
 * @param outPath path to write header (e.g. "magic_constants.hpp")
 * @param arrayNamePrefix optional prefix used for generated symbol names
 * @return true on success, false on failure (e.g. cannot open file)
 */
bool serialize_magics_to_header(const std::string& outPath,
                                const std::string& arrayNamePrefix = "s");

}  // namespace lilia::model::magic
