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

enum class Bound : std::uint8_t { Exact = 0, Lower = 1, Upper = 2 };

struct TTEntry4 {
  std::uint64_t key = 0;
  int32_t value = 0;
  int16_t depth = std::numeric_limits<int16_t>::min();
  Bound bound = Bound::Exact;
  Move best;
  uint8_t age = 0;
};

// Ausrichtungs-Padding zur Reduktion von False-Sharing zwischen Clustern
struct alignas(64) Cluster {
  std::array<TTEntry4, 4> e{};

  // Einfacher Spinlock pro Cluster – korrekt, aber kann bei sehr vielen Threads ein Hotspot werden
  mutable std::atomic_flag lock = ATOMIC_FLAG_INIT;

  struct ClusterLock {
    const Cluster &cluster;
    explicit ClusterLock(const Cluster &c) : cluster(c) {
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

    ClusterLock(const ClusterLock &) = delete;
    ClusterLock &operator=(const ClusterLock &) = delete;
  };

  inline ClusterLock lockCluster() const { return ClusterLock(*this); }
};

class TT4 {
 public:
  explicit TT4(std::size_t mb = 16) { resize(mb); }

  // Größte Zweierpotenz <= x (robust, ohne Off-by-One)
  static std::size_t highest_power_of_two(std::size_t x) {
    if (x == 0) return 1;
    std::size_t p = 1;
    while ((p << 1) && ((p << 1) <= x)) p <<= 1;
    return p;
  }

  // Nicht nebenläufig zu probe()/store() verwenden
  void resize(std::size_t mb) {
    bytes = mb * 1024ULL * 1024ULL;
    std::size_t requested = bytes / sizeof(Cluster);
    if (requested == 0) requested = 1;

    slots = highest_power_of_two(requested);
    if (slots == 0) slots = 1;  // Sicherheitsnetz

    m_table = std::vector<Cluster>(slots);

    generation = 1;
  }

  // Nicht nebenläufig zu probe()/store() verwenden
  void clear() {
    for (auto &c : m_table) {
      auto lk = c.lockCluster();
      for (auto &entry : c.e) entry = TTEntry4{};
    }
    generation = 0;  // nach nächstem new_generation() wird auf 1 gesetzt
  }

  std::optional<TTEntry4> probe(std::uint64_t key) const {
    Cluster &c = m_table[index(key)];
    auto lk = c.lockCluster();
    for (auto &entry : c.e) {
      if (entry.key == key) {
        return std::optional<TTEntry4>(entry);
      }
    }
    return std::nullopt;
  }

  void store(std::uint64_t key, int32_t value, int16_t depth, Bound bound, const Move &best) {
    Cluster &c = m_table[index(key)];
    auto lk = c.lockCluster();

    const uint8_t curGen = static_cast<uint8_t>(generation);  // implizit mod 256

    // 1) Update, falls Key bereits vorhanden
    for (auto &entry : c.e) {
      if (entry.key == key) {
        entry.key = key;
        entry.value = value;
        entry.depth = depth;
        entry.bound = bound;
        entry.best = best;
        entry.age = curGen;
        return;
      }
    }

    // 2) Freier Slot?
    for (auto &entry : c.e) {
      if (entry.depth == std::numeric_limits<int16_t>::min()) {
        entry.key = key;
        entry.value = value;
        entry.depth = depth;
        entry.bound = bound;
        entry.best = best;
        entry.age = curGen;
        return;
      }
    }

    // 3) Ersetzungsheuristik: möglichst "schlechter" (flach + alt)
    int idx = 0;
    auto score_of = [&](const TTEntry4 &en) {
      int ageDelta = static_cast<uint8_t>(curGen - en.age);
      return static_cast<int>(en.depth) * 256 - ageDelta;  // <--- minus!
    };

    int bestScore = score_of(c.e[0]);
    for (int i = 1; i < 4; ++i) {
      int s = score_of(c.e[i]);
      if (s < bestScore) {
        bestScore = s;
        idx = i;
      }
    }

    c.e[idx].key = key;
    c.e[idx].value = value;
    c.e[idx].depth = depth;
    c.e[idx].bound = bound;
    c.e[idx].best = best;
    c.e[idx].age = curGen;
  }

  // Empfehlung: am Beginn jeder Iteration (iterative deepening) aufrufen
  void new_generation() {
    ++generation;

    // Wrap: alle Altersmarker auf 0 setzen, Generation auf 1 (0 reserviert)
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
  uint32_t generation = 1;  // nur die unteren 8 Bit werden für age benutzt

  inline std::size_t index(std::uint64_t key) const {
    assert((slots & (slots - 1)) == 0 && slots != 0);
    // Maske über die niedrigstwertigen Bits – schnell und ok; optional könnte man hier noch ein
    // Mixen des Keys einbauen
    return static_cast<std::size_t>(key) & (slots - 1);
  }
};

}  // namespace lilia::model
