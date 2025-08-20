#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "bitboard.hpp"
#include "model_types.hpp"

namespace lilia::model::magic {

enum class Slider { Rook, Bishop };

struct Magic {
  bb::Bitboard magic = 0ULL;
  std::uint8_t shift = 0;  // number of bits to shift (usually 64 - relevant_bits)
};

/**
 * @brief Initialize magic tables.
 *
 * Behavior:
 * - If a generated header is available (serializer emits LILIA_MAGIC_HAVE_CONSTANTS),
 *   constants are loaded from it.
 * - Otherwise, masks are computed and magics + attack tables are generated at runtime.
 */
void init_magics();

/** Query sliding attacks using precomputed magic tables. */
bb::Bitboard sliding_attacks(Slider s, core::Square sq, bb::Bitboard occ);

/** Expose masks/magics/tables for serialization and debugging. */
const std::array<bb::Bitboard, 64>& rook_masks();
const std::array<bb::Bitboard, 64>& bishop_masks();
const std::array<Magic, 64>& rook_magics();
const std::array<Magic, 64>& bishop_magics();
const std::array<std::vector<bb::Bitboard>, 64>& rook_tables();
const std::array<std::vector<bb::Bitboard>, 64>& bishop_tables();

}  // namespace lilia::model::magic
