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
  std::uint8_t shift = 0;
};

/// Precomputed tables (filled by init)
void init_magics();  // builds masks, attack tables (calls generator if no constants present)

/// Attack query
bb::Bitboard sliding_attacks(Slider s, core::Square sq, bb::Bitboard occ);

/// Expose masks (useful for generation/debug
const std::array<bb::Bitboard, 64>& rook_masks();
const std::array<bb::Bitboard, 64>& bishop_masks();
const std::array<Magic, 64>& rook_magics();
const std::array<Magic, 64>& bishop_magics();
const std::array<std::vector<bb::Bitboard>, 64>& rook_tables();
const std::array<std::vector<bb::Bitboard>, 64>& bishop_tables();

}  // namespace lilia::model::magic
