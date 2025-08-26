#pragma once
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <thread>
#include <vector>

#include "move.hpp"

namespace lilia::model {

// Bound types for TT entries
enum class Bound : std::uint8_t { Exact = 0, Lower = 1, Upper = 2 };

// Single TT entry
struct TTEntry4 {
  std::uint64_t key = 0;
  int32_t value = 0;
  int16_t depth = std::numeric_limits<int16_t>::min();  // -32768
  Bound bound = Bound::Exact;
  Move best;
  uint8_t age = 0;  // generation counter low bits (kept for compactness)
};

// Cluster of 4 TT entries with lightweight spinlock
struct Cluster {
  std::array<TTEntry4, 4> e{};
  // spinlock for this cluster (atomic_flag is NOT copyable)
  mutable std::atomic_flag lock = ATOMIC_FLAG_INIT;

  // RAII lock guard for cluster
  struct ClusterLock {
    const Cluster &cluster;
    ClusterLock(const Cluster &c) : cluster(c) {
      // acquire with adaptive backoff
      int loops = 0;
      while (cluster.lock.test_and_set(std::memory_order_acquire)) {
        ++loops;
        if (loops < 4) {
          std::this_thread::yield();
        } else if (loops < 16) {
          std::this_thread::sleep_for(std::chrono::nanoseconds(50));
        } else {
          std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
      }
    }
    ~ClusterLock() { cluster.lock.clear(std::memory_order_release); }
    // disable copying/moving
    ClusterLock(const ClusterLock &) = delete;
    ClusterLock &operator=(const ClusterLock &) = delete;
  };

  // convenience helpers (thin wrappers)
  inline ClusterLock lockCluster() const { return ClusterLock(*this); }
};

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

    // IMPORTANT: construct a fresh vector of Clusters (no copy/assign of atomic_flag)
    m_table = std::vector<Cluster>(slots);

    generation = 1;
  }

  void clear() {
    // reset entries and ensure cluster locks are released
    for (auto &c : m_table) {
      // acquire the cluster lock via RAII to safely modify entries
      auto lk = c.lockCluster();
      for (auto &entry : c.e) entry = TTEntry4{};
      // lock released by guard destructor (memory_order_release)
    }
    generation = 0;
  }

  // thread-safe probe (returns a copy of the entry if found)
  std::optional<TTEntry4> probe(std::uint64_t key) const {
    Cluster &c = m_table[index(key)];
    auto lk = c.lockCluster();
    for (auto &entry : c.e) {
      if (entry.key == key) {
        // return a copy while still holding the lock (optional), then unlock via RAII
        return std::optional<TTEntry4>(entry);
      }
    }
    return std::nullopt;
  }

  // thread-safe store (locks cluster briefly); replacement policy same as before
  void store(std::uint64_t key, int32_t value, int16_t depth, Bound bound, const Move &best) {
    Cluster &c = m_table[index(key)];
    auto lk = c.lockCluster();

    // update existing entry if key matches
    for (auto &entry : c.e) {
      if (entry.key == key) {
        entry.key = key;
        entry.value = value;
        entry.depth = depth;
        entry.bound = bound;
        entry.best = best;
        entry.age = static_cast<uint8_t>(generation & 0xFF);
        return;
      }
    }

    // find empty slot
    for (auto &entry : c.e) {
      if (entry.depth == std::numeric_limits<int16_t>::min()) {
        entry.key = key;
        entry.value = value;
        entry.depth = depth;
        entry.bound = bound;
        entry.best = best;
        entry.age = static_cast<uint8_t>(generation & 0xFF);
        return;
      }
    }

    // replacement: pick lowest score to replace (depth * 256 + age diff heuristic)
    int idx = 0;
    int bestScore =
        static_cast<int>(c.e[0].depth) * 256 + static_cast<int>((generation & 0xFF) - c.e[0].age);
    for (int i = 1; i < 4; i++) {
      int score =
          static_cast<int>(c.e[i].depth) * 256 + static_cast<int>((generation & 0xFF) - c.e[i].age);
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
    c.e[idx].age = static_cast<uint8_t>(generation & 0xFF);
  }

  void new_generation() {
    ++generation;
    // very rarely: if wrap-around occurred, clear ages to avoid negative diffs
    if (generation == 0) {
      for (auto &c : m_table) {
        auto lk = c.lockCluster();
        for (auto &entry : c.e) entry.age = 0;
      }
      generation = 1;
    }
  }

 private:
  mutable std::vector<Cluster> m_table;
  std::size_t slots = 0;
  std::size_t bytes = 0;
  uint32_t generation = 1;  // larger counter to reduce frequency of wraparound

  inline std::size_t index(std::uint64_t key) const {
    assert((slots & (slots - 1)) == 0 && slots != 0);
    return static_cast<std::size_t>(key) & (slots - 1);
  }
};

}  // namespace lilia::model
