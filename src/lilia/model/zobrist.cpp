#include "lilia/model/zobrist.hpp"

#include <atomic>
#include <mutex>

namespace lilia::model {

// Static storage
bb::Bitboard Zobrist::piece[2][6][64];
bb::Bitboard Zobrist::castling[16];
bb::Bitboard Zobrist::epFile[8];
bb::Bitboard Zobrist::side;
bb::Bitboard Zobrist::epCaptureMask[2][64];

namespace {
// SplitMix64: schnell, gute Streuung, deterministisch
static inline std::uint64_t splitmix64(std::uint64_t& x) {
  std::uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

// Thread-sichere Einmal-Init für Zobrist::init()
std::once_flag g_once_init;

// EP-Capture-Masken vorbereiten:
// Für ein Ziel 't' (EP-Square) kommen Angreifer von:
//  - Weiß: SW(t) | SE(t)
//  - Schwarz: NW(t) | NE(t)
static constexpr void build_ep_capture_masks() {
  for (int s = 0; s < 64; ++s) {
    const bb::Bitboard t = bb::sq_bb(static_cast<core::Square>(s));
    Zobrist::epCaptureMask[bb::ci(core::Color::White)][s] = bb::sw(t) | bb::se(t);
    Zobrist::epCaptureMask[bb::ci(core::Color::Black)][s] = bb::nw(t) | bb::ne(t);
  }
}
}  // namespace

void Zobrist::init(std::uint64_t seed) {
  // Fülle alle Keys; vermeide 0-Werte (sehr selten, aber sicher ist sicher)
  auto next = [&]() {
    std::uint64_t v;
    do {
      v = splitmix64(seed);
    } while (v == 0);
    return v;
  };

  for (int c = 0; c < 2; ++c) {
    for (int t = 0; t < 6; ++t) {
      for (int s = 0; s < 64; ++s) {
        piece[c][t][s] = next();
      }
    }
  }

  for (int i = 0; i < 16; ++i) castling[i] = next();
  for (int f = 0; f < 8; ++f) epFile[f] = next();

  side = next();

  // EP-Angriffsquellen vorbereiten (deterministisch, unabhängig vom Seed)
  build_ep_capture_masks();
}

void Zobrist::init() {
  // Fester, reproduzierbarer Seed
  std::call_once(g_once_init, [] { Zobrist::init(0xC0FFEE123456789ULL); });
}

}  // namespace lilia::model
