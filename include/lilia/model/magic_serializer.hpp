#pragma once
#include <string>

namespace lilia::model::magic {

bool serialize_magics_to_header(const std::string& outPath,
                                const std::string& arrayNamePrefix = "s");

}
