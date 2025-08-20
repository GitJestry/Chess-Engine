#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "move.hpp"

namespace lilia::model {

// Bound types for TT entries
enum class Bound : std::uint8_t { Exact = 0, Lower = 1, Upper = 2 };

// Single TT entry
struct TTEntry4 {
  std::uint64_t key = 0;
  int32_t value = 0;
  int16_t depth = -32768;
  Bound bound = Bound::Exact;
  Move best;
  uint8_t age = 0;  // generation counter low bits
};

// Cluster of 4 TT entries
struct Cluster {
  std::array<TTEntry4, 4> e{};
};

// Transposition table with clusters of 4 entries
class TT4 {
 public:
  TT4(std::size_t mb = 16) { resize(mb); }

  // helper: highest power of two <= x
  static std::size_t highest_power_of_two(std::size_t x) {
    if (x == 0) return 1;
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
#if SIZE_MAX > UINT32_MAX
    x |= x >> 32;
#endif
    ++x;
    return x >> 1;  // previous power of two
  }

  void resize(std::size_t mb) {
    bytes = mb * 1024ULL * 1024ULL;
    std::size_t requested = bytes / sizeof(Cluster);
    if (requested == 0) requested = 1;
    // make slots a power of two (highest power of two <= requested)
    slots = highest_power_of_two(requested);
    table.assign(slots, Cluster());
    generation = 1;
  }

  void clear() {
    for (auto &c : table)
      for (auto &e : c.e) e = TTEntry4{};
    generation = 0;
  }

  TTEntry4 *probe(std::uint64_t key) {
    Cluster &c = table[index(key)];
    for (auto &e : c.e)
      if (e.key == key) return &e;
    return nullptr;
  }

  void store(std::uint64_t key, int32_t value, int16_t depth, Bound bound, const Move &best) {
    Cluster &c = table[index(key)];

    for (auto &e : c.e) {
      if (e.key == key) {
        e.key = key;
        e.value = value;
        e.depth = depth;
        e.bound = bound;
        e.best = best;
        e.age = generation;
        return;
      }
    }

    for (auto &e : c.e) {
      if (e.depth == -32768) {
        e.key = key;
        e.value = value;
        e.depth = depth;
        e.bound = bound;
        e.best = best;
        e.age = generation;
        return;
      }
    }

    int idx = 0;
    int bestScore = (int)c.e[0].depth * 256 + (int)(generation - c.e[0].age);
    for (int i = 1; i < 4; i++) {
      int score = (int)c.e[i].depth * 256 + (int)(generation - c.e[i].age);
      if (score < bestScore) {
        bestScore = score;
        idx = i;
      }
    }
    c.e[idx].key = key;
    c.e[idx].value = value;
    c.e[idx].depth = depth;
    c.e[idx].bound = bound;
    c.e[idx].best = best;
    c.e[idx].age = generation;
  }

  void new_generation() {
    ++generation;
    if (generation == 0) {
      for (auto &c : table)
        for (auto &e : c.e) e.age = 0;
      generation = 1;
    }
  }

 private:
  std::vector<Cluster> table;
  std::size_t slots = 0;
  std::size_t bytes = 0;
  uint8_t generation = 1;

  inline std::size_t index(std::uint64_t key) const {
    return static_cast<std::size_t>(key) & (slots - 1);
  }
};

}  // namespace lilia::model
