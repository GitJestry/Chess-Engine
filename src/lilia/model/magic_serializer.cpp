#include "lilia/model/magic_serializer.hpp"

namespace lilia::model::magic {

bool serialize_magics_to_header(const std::string& /*outPath*/, const std::string& /*prefix*/) {
  // Serialization of precomputed magic bitboards is not required during normal builds.
  // This stub keeps the build functional even when generation is unused.
  return false;
}

}  // namespace lilia::model::magic
