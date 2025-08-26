#include "lilia/model/core/magic.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

#include "lilia/model/core/random.hpp"
#include "lilia/model/generated/magic_constants.hpp"

namespace lilia::model::magic {

static std::array<bb::Bitboard, 64> g_rook_mask{};
static std::array<bb::Bitboard, 64> g_bishop_mask{};

static std::array<Magic, 64> g_rook_magic{};
static std::array<Magic, 64> g_bishop_magic{};

static std::array<std::vector<bb::Bitboard>, 64> g_rook_table;
static std::array<std::vector<bb::Bitboard>, 64> g_bishop_table;

template <class F>
static inline void foreach_subset(bb::Bitboard mask, F&& f) {
  bb::Bitboard sub = mask;
  while (true) {
    f(sub);
    if (sub == 0) break;
    sub = (sub - 1) & mask;
  }
}

static inline bb::Bitboard brute_rook(core::Square sq, bb::Bitboard occ) {
  return bb::rook_attacks(sq, occ);
}
static inline bb::Bitboard brute_bishop(core::Square sq, bb::Bitboard occ) {
  return bb::bishop_attacks(sq, occ);
}
static inline bb::Bitboard brute_attacks(Slider s, core::Square sq, bb::Bitboard occ) {
  return (s == Slider::Rook) ? brute_rook(sq, occ) : brute_bishop(sq, occ);
}

static inline uint64_t index_for_occ(bb::Bitboard occ, bb::Bitboard mask, bb::Bitboard magic,
                                     uint8_t shift) {
  const int bits = bb::popcount(mask);
  if (bits == 0) return 0ULL;
  const bb::Bitboard subset = occ & mask;
  
  const uint64_t raw = static_cast<uint64_t>((subset * magic) >> shift);
  const uint64_t mask_idx =
      (bits >= 64) ? std::numeric_limits<uint64_t>::max() : ((1ULL << bits) - 1ULL);
  return raw & mask_idx;
}

static inline void build_table_for_square(Slider s, int sq, bb::Bitboard mask, bb::Bitboard magic,
                                          uint8_t shift, std::vector<bb::Bitboard>& outTable) {
  const int bits = bb::popcount(mask);
  const size_t tableSize = (bits >= 64) ? 0 : (1ULL << bits);
  const size_t allocSize = (tableSize == 0) ? 1 : tableSize;
  outTable.assign(allocSize, 0ULL);

  foreach_subset(mask, [&](bb::Bitboard occSubset) {
    const uint64_t index = index_for_occ(occSubset, mask, magic, shift);
    outTable[index] = brute_attacks(s, static_cast<core::Square>(sq), occSubset);
  });
}

static inline bool try_magic_for_square(Slider s, int sq, bb::Bitboard mask, bb::Bitboard magic,
                                        uint8_t shift, std::vector<bb::Bitboard>& outTable) {
  const int bits = bb::popcount(mask);
  const size_t tableSize = (bits >= 64) ? 0 : (1ULL << bits);
  const size_t allocSize = (tableSize == 0) ? 1 : tableSize;

  
  std::vector<char> used(allocSize);
  std::vector<bb::Bitboard> table(allocSize);

  
  bb::Bitboard occSubset = mask;
  while (true) {
    const uint64_t idx = index_for_occ(occSubset, mask, magic, shift);
    const bb::Bitboard atk = brute_attacks(s, static_cast<core::Square>(sq), occSubset);

    if (!used[idx]) {
      used[idx] = 1;
      table[idx] = atk;
    } else if (table[idx] != atk) {
      
      return false;
    }

    if (occSubset == 0) break;
    occSubset = (occSubset - 1) & mask;
  }

  
  outTable = std::move(table);
  return true;
}

static inline bool find_magic_for_square(Slider s, int sq, bb::Bitboard mask,
                                         bb::Bitboard& out_magic, uint8_t& out_shift,
                                         std::vector<bb::Bitboard>& outTable) {
  const int bits = bb::popcount(mask);
  const uint8_t shift =
      static_cast<uint8_t>((bits == 0) ? 64u : (64u - static_cast<uint8_t>(bits)));
  out_shift = shift;

  bb::Bitboard seed = 0xC0FFEE123456789ULL ^ (static_cast<bb::Bitboard>(sq) << 32) ^
                      (s == Slider::Rook ? 0xF0F0F0F0ULL : 0x0F0F0F0FULL);

  
  constexpr int MAX_ATTEMPTS = 2000000;

  
  random::SplitMix64 splitmix(seed);

  
  auto gen_candidate = [&](int strategy) -> bb::Bitboard {
    switch (strategy) {
      case 0:
        return splitmix.next() & splitmix.next() & splitmix.next();  
      case 1:
        return splitmix.next() & splitmix.next();  
      case 2:
        return splitmix.next() ^ (splitmix.next() << 1);  
      case 3: {  
        bb::Bitboard v = splitmix.next() & splitmix.next();
        bb::Bitboard hi = (splitmix.next() & 0xFFULL) << 56;
        return v | hi;
      }
      default:
        return splitmix.next();
    }
  };

  for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
    
    for (int strat = 0; strat < 4; ++strat) {
      bb::Bitboard cand = gen_candidate(strat);

      
      if (bits > 0) {
        const int highpop = bb::popcount((cand * mask) & 0xFF00000000000000ULL);
        if (highpop < 2) continue;  
      }

      if (try_magic_for_square(s, sq, mask, cand, shift, outTable)) {
        out_magic = cand;
        return true;
      }
    }

    
    if ((attempt & 0xFFF) == 0) {

#ifndef NDEBUG
      std::cerr << "find_magic for sq=" << sq << " attempt=" << attempt << "\n";
#endif
    }
  }

#ifndef NDEBUG
  std::cerr << "find_magic_for_square FAILED (sq=" << sq << ", bits=" << bits
            << ", MAX_ATTEMPTS=" << MAX_ATTEMPTS << ")\n";
#endif
  return false;
}

static inline bb::Bitboard rook_relevant_mask(core::Square sq) {
  bb::Bitboard mask = 0ULL;
  int r = bb::rank_of(sq), f = bb::file_of(sq);

  for (int rr = r + 1; rr <= 6; ++rr) mask |= bb::sq_bb(static_cast<core::Square>(rr * 8 + f));
  for (int rr = r - 1; rr >= 1; --rr) mask |= bb::sq_bb(static_cast<core::Square>(rr * 8 + f));
  for (int ff = f + 1; ff <= 6; ++ff) mask |= bb::sq_bb(static_cast<core::Square>(r * 8 + ff));
  for (int ff = f - 1; ff >= 1; --ff) mask |= bb::sq_bb(static_cast<core::Square>(r * 8 + ff));

  return mask;
}

static inline bb::Bitboard bishop_relevant_mask(core::Square sq) {
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

static inline void build_masks() {
  for (int sq = 0; sq < 64; ++sq) {
    g_rook_mask[sq] = rook_relevant_mask(static_cast<core::Square>(sq));
    g_bishop_mask[sq] = bishop_relevant_mask(static_cast<core::Square>(sq));
  }
}

static inline void generate_all_magics_and_tables() {
  build_masks();

  
  for (int sq = 0; sq < 64; ++sq) {
    std::vector<bb::Bitboard> table;
    bb::Bitboard magic = 0ULL;
    uint8_t shift = 0;
    const bb::Bitboard mask = g_rook_mask[sq];
    const bool ok = find_magic_for_square(Slider::Rook, sq, mask, magic, shift, table);
    if (!ok) {
      std::cerr << "Failed to find rook magic for sq " << sq << " (bits=" << bb::popcount(mask)
                << ")\n";
      
    }
    g_rook_magic[sq] = Magic{magic, shift};
    g_rook_table[sq] = std::move(table);
  }

  
  for (int sq = 0; sq < 64; ++sq) {
    std::vector<bb::Bitboard> table;
    bb::Bitboard magic = 0ULL;
    uint8_t shift = 0;
    const bb::Bitboard mask = g_bishop_mask[sq];
    const bool ok = find_magic_for_square(Slider::Bishop, sq, mask, magic, shift, table);
    if (!ok) {
      std::cerr << "Failed to find bishop magic for sq " << sq << " (bits=" << bb::popcount(mask)
                << ")\n";
      
    }
    g_bishop_magic[sq] = Magic{magic, shift};
    g_bishop_table[sq] = std::move(table);
  }
}

void init_magics() {
#ifdef LILIA_MAGIC_HAVE_CONSTANTS
  using namespace lilia::model::magic::constants;

  
  build_masks();

  for (int i = 0; i < 64; ++i) {
    g_rook_magic[i].magic = s_rook_magic[i].magic;
    g_rook_magic[i].shift = s_rook_magic[i].shift;

    g_bishop_magic[i].magic = s_bishop_magic[i].magic;
    g_bishop_magic[i].shift = s_bishop_magic[i].shift;

    g_rook_table[i] = s_rook_table[i];
    g_bishop_table[i] = s_bishop_table[i];
  }
#else
  generate_all_magics_and_tables();

#endif
}

bb::Bitboard sliding_attacks(Slider s, core::Square sq, bb::Bitboard occ) {
  const int i = static_cast<int>(sq);

  if (s == Slider::Rook) {
    const bb::Bitboard mask = g_rook_mask[i];
    const uint64_t idx = index_for_occ(occ, mask, g_rook_magic[i].magic, g_rook_magic[i].shift);
    return g_rook_table[i][idx];
  } else {
    const bb::Bitboard mask = g_bishop_mask[i];
    const uint64_t idx = index_for_occ(occ, mask, g_bishop_magic[i].magic, g_bishop_magic[i].shift);
    return g_bishop_table[i][idx];
  }
}

const std::array<bb::Bitboard, 64>& rook_masks() {
  return g_rook_mask;
}
const std::array<bb::Bitboard, 64>& bishop_masks() {
  return g_bishop_mask;
}
const std::array<Magic, 64>& rook_magics() {
  return g_rook_magic;
}
const std::array<Magic, 64>& bishop_magics() {
  return g_bishop_magic;
}
const std::array<std::vector<bb::Bitboard>, 64>& rook_tables() {
  return g_rook_table;
}
const std::array<std::vector<bb::Bitboard>, 64>& bishop_tables() {
  return g_bishop_table;
}

}  
