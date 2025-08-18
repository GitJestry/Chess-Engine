#include "lilia/model/core/magic.hpp"

#include <algorithm>
#include <cassert>
#include <functional>
#include <random>
#include <unordered_map>

#include "lilia/model/generated/magic_constants.hpp"

namespace lilia::model::magic {

static std::array<bb::Bitboard, 64> s_rook_mask{};
static std::array<bb::Bitboard, 64> s_bishop_mask{};

static std::array<Magic, 64> s_rook_magic{};
static std::array<Magic, 64> s_bishop_magic{};

static std::array<std::vector<bb::Bitboard>, 64> s_rook_table;
static std::array<std::vector<bb::Bitboard>, 64> s_bishop_table;

/// Small RNG (splitmix64)
static bb::Bitboard splitmix_next(bb::Bitboard& x) {
  uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

// build mask: relevant occupancy bits for sliding attacks (exclude border bits)
static bb::Bitboard rook_relevant_mask(core::Square sq) {
  bb::Bitboard mask = 0ULL;
  int r = bb::rank_of(sq), f = bb::file_of(sq);

  // north
  for (int rr = r + 1; rr <= 6; ++rr) mask |= bb::sq_bb(static_cast<core::Square>(rr * 8 + f));
  // south
  for (int rr = r - 1; rr >= 1; --rr) mask |= bb::sq_bb(static_cast<core::Square>(rr * 8 + f));
  // east
  for (int ff = f + 1; ff <= 6; ++ff) mask |= bb::sq_bb(static_cast<core::Square>(r * 8 + ff));
  // west
  for (int ff = f - 1; ff >= 1; --ff) mask |= bb::sq_bb(static_cast<core::Square>(r * 8 + ff));

  return mask;
}

static bb::Bitboard bishop_relevant_mask(core::Square sq) {
  bb::Bitboard mask = 0ULL;
  int r = bb::rank_of(sq), f = bb::file_of(sq);

  for (int rr = r + 1, ff = f + 1; rr <= 6 && ff <= 6; ++rr, ++ff)
    mask |= bb::sq_bb(static_cast<core::Square>(rr * 8 + ff));
  for (int rr = r + 1, ff = f - 1; rr <= 6 && ff >= 1; ++rr, --ff)
    mask |= bb::sq_bb(static_cast<core::Square>(rr * 8 + ff));
  for (int rr = r - 1, ff = f + 1; rr >= 1 && ff <= 6; --rr, ++ff)
    mask |= bb::sq_bb(static_cast<core::Square>(rr * 8 + ff));
  for (int rr = r - 1, ff = f - 1; rr >= 1 && ff >= 1; --rr, --ff)
    mask |= bb::sq_bb(static_cast<core::Square>(rr * 8 + ff));

  return mask;
}

// compute attack bitboard for a given occupancy (ray stops at blockers)
static bb::Bitboard sliding_attacks(Slider s, core::Square sq, bb::Bitboard occ) {
  if (s == Slider::Rook) return bb::rook_attacks(sq, occ);
  return bb::bishop_attacks(sq, occ);
}

// iterate submasks of mask: classic subset iteration
static void foreach_subset(bb::Bitboard mask, const std::function<void(bb::Bitboard)>& f) {
  bb::Bitboard subset = mask;
  while (true) {
    f(subset);
    if (subset == 0) break;
    subset = (subset - 1) & mask;
  }
}

// try candidate magic and produce tables; returns true if collision-free
static bool try_magic_for_square(Slider s, int sq, bb::Bitboard magic_candidate, int shift,
                                 std::vector<bb::Bitboard>& outTable) {
  bb::Bitboard mask = (s == Slider::Rook) ? s_rook_mask[sq] : s_bishop_mask[sq];
  int bits = bb::popcount(mask);
  size_t tableSize = 1ULL << bits;
  outTable.assign(tableSize, 0ULL);
  // mapping occupancy -> attacks
  std::unordered_map<bb::Bitboard, bb::Bitboard> used;

  foreach_subset(mask, [&](bb::Bitboard occSubset) {
    // compress occupancy into index by enumerating bits in mask
    // but easier: generate the occupancy with full bit positions and compute index by magic
    bb::Bitboard occ = occSubset;
    bb::Bitboard index = ((occ * magic_candidate) >> shift);
    if (used.count(index)) {
      if (used[index] != sliding_attacks(s, sq, occ)) {
        // collision
        used[index] = used[index];  // no-op
      }
    } else {
      used[index] = sliding_attacks(s, sq, occ);
    }
  });

  // Check collisions vs table size: if any index >= tableSize -> fail
  for (auto& p : used) {
    if (p.first >= tableSize) return false;
    if (outTable[p.first] == 0ULL)
      outTable[p.first] = p.second;
    else if (outTable[p.first] != p.second)
      return false;
  }
  return true;
}

// find magic by random search (slow) one time call
bool find_magic_for_square(Slider s, int sq, bb::Bitboard& out_magic, std::uint8_t& out_shift) {
  // prepare masks if not ready
  if (s_rook_mask[sq] == 0ULL && s == Slider::Rook) s_rook_mask[sq] = rook_relevant_mask(sq);
  if (s_bishop_mask[sq] == 0ULL && s == Slider::Bishop)
    s_bishop_mask[sq] = bishop_relevant_mask(sq);

  bb::Bitboard mask = (s == Slider::Rook) ? s_rook_mask[sq] : s_bishop_mask[sq];
  int bits = bb::popcount(mask);
  int shift = 64 - bits;
  out_shift = static_cast<std::uint8_t>(shift);

  // Candidate generation via splitmix-like
  bb::Bitboard seed = 0xC0FFEE123456789ULL ^ ((bb::Bitboard)sq << 32) ^
                      (s == Slider::Rook ? 0xF0F0F0F0ULL : 0x0F0F0F0FULL);
  for (int attempt = 0; attempt < 1000000; ++attempt) {
    bb::Bitboard cand =
        splitmix_next(seed) & splitmix_next(seed) & splitmix_next(seed);  // bias to sparse
    std::vector<bb::Bitboard> tmp;
    if (try_magic_for_square(s, sq, cand, shift, tmp)) {
      out_magic = cand;
      out_shift = static_cast<std::uint8_t>(shift);
      return true;
    }
  }
  return false;
}

void init_magics() {
  for (int i = 0; i < 64; ++i) {
    s_rook_magic[i].magic = constants::s_rook_magic[i].magic;
    s_rook_magic[i].shift = constants::s_rook_magic[i].shift;

    s_bishop_magic[i].magic = constants::s_bishop_magic[i].magic;
    s_bishop_magic[i].shift = constants::s_bishop_magic[i].shift;

    s_rook_table[i] = constants::s_rook_table[i];
    s_bishop_table[i] = constants::s_bishop_table[i];
  }
}

// accessors for serializer
const std::array<bb::Bitboard, 64>& rook_masks() {
  return s_rook_mask;
}
const std::array<bb::Bitboard, 64>& bishop_masks() {
  return s_bishop_mask;
}
const std::array<Magic, 64>& rook_magics() {
  return s_rook_magic;
}
const std::array<Magic, 64>& bishop_magics() {
  return s_bishop_magic;
}
const std::array<std::vector<bb::Bitboard>, 64>& rook_tables() {
  return s_rook_table;
}
const std::array<std::vector<bb::Bitboard>, 64>& bishop_tables() {
  return s_bishop_table;
}

}  // namespace lilia::model::magic
