#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>

#include "move.hpp"

namespace lilia::model {

// Bound/Entry bleiben kompatibel
enum class Bound : std::uint8_t { Exact = 0, Lower = 1, Upper = 2 };

struct TTEntry4 {
  std::uint64_t key = 0;
  int32_t value = 0;
  int16_t depth = 0;
  Bound bound = Bound::Exact;
  Move best{};
  std::uint8_t age = 0;
};

// Ein Slot mit atomaren Feldern + Seqlock (even=stabil, odd=Writer aktiv)
struct PackedEntry {
  std::atomic<std::uint64_t> key{0};      // 0 => leer (Publikationsflag bei Neu-Schreiben)
  std::atomic<std::uint64_t> payload{0};  // value/depth/bound/age gepackt
  std::atomic<std::uint32_t> mv{0};       // from/to/promo/castle/ep gepackt
  std::atomic<std::uint32_t> seq{0};      // Seqlock-Zähler

  PackedEntry() = default;
  PackedEntry(const PackedEntry&) = delete;
  PackedEntry& operator=(const PackedEntry&) = delete;
  PackedEntry(PackedEntry&&) = delete;
  PackedEntry& operator=(PackedEntry&&) = delete;
};

struct alignas(64) Cluster {
  std::array<PackedEntry, 4> e{};
  Cluster() = default;
  Cluster(const Cluster&) = delete;
  Cluster& operator=(const Cluster&) = delete;
  Cluster(Cluster&&) = delete;
  Cluster& operator=(Cluster&&) = delete;
};

// (Optional) sehr leichte Prefetch-Hilfe – cross-platform safe no-op
#if defined(__GNUC__) || defined(__clang__)
#define LILIA_PREFETCH_L1(ptr) __builtin_prefetch((ptr), 0, 3)
#else
#define LILIA_PREFETCH_L1(ptr) ((void)0)
#endif

class TT4 {
 public:
  explicit TT4(std::size_t mb = 16) { resize(mb); }

  // nicht parallel zum Suchen aufrufen
  void resize(std::size_t mb) {
    std::size_t bytes = mb * 1024ULL * 1024ULL;
    if (bytes < sizeof(Cluster)) bytes = sizeof(Cluster);
    std::size_t requested = bytes / sizeof(Cluster);
    slots_ = highest_power_of_two(requested);
    if (slots_ == 0) slots_ = 1;

    table_.reset();
    table_ = std::unique_ptr<Cluster[]>(new Cluster[slots_]);
    generation_.store(1, std::memory_order_relaxed);
  }

  // nicht parallel zum Suchen aufrufen
  void clear() {
    auto oldSlots = slots_;
    table_.reset();
    table_ = std::unique_ptr<Cluster[]>(new Cluster[oldSlots]);
    generation_.store(1, std::memory_order_relaxed);
  }

  // Konsistente Probe dank Seqlock + Key-Recheck
  std::optional<TTEntry4> probe(std::uint64_t key) const {
    const Cluster& c = table_[index(key)];
    LILIA_PREFETCH_L1(&c);

    for (const auto& ent : c.e) {
      const std::uint64_t k1 = ent.key.load(std::memory_order_acquire);
      if (k1 != key) continue;

      // kleine Retry-Schleife, falls Writer gerade aktiv ist
      for (int tries = 0; tries < 3; ++tries) {
        const std::uint32_t s1 = ent.seq.load(std::memory_order_acquire);
        if (s1 & 1u) {  // Writer aktiv
          continue;     // nächster Try / Slot
        }

        const std::uint64_t pay = ent.payload.load(std::memory_order_acquire);
        const std::uint32_t mv = ent.mv.load(std::memory_order_acquire);

        const std::uint32_t s2 = ent.seq.load(std::memory_order_acquire);
        const std::uint64_t k2 = ent.key.load(std::memory_order_acquire);

        // Konsistenzcheck: gleicher Key, stabiler Seqlock, gültiger Payload
        if (k2 != k1 || (s2 != s1) || (s2 & 1u) || pay == 0) {
          continue;
        }

        TTEntry4 out{};
        out.key = key;
        unpack_payload(pay, out.value, out.depth, out.bound, out.age);
        out.best = unpack_move(mv);
        return out;
      }
    }
    return std::nullopt;
  }

  // Thread-sicheres Store mit Seqlock
  void store(std::uint64_t key, int32_t value, int16_t depth, Bound bound, const Move& best) {
    Cluster& c = table_[index(key)];
    const std::uint8_t curAge =
        static_cast<std::uint8_t>(generation_.load(std::memory_order_relaxed));

    const auto write_entry_newkey = [&](PackedEntry& ent) {
      // Neu-Publikation: zuerst Seqlock öffnen (odd), Daten schreiben, Key publishen, Seqlock
      // schließen
      const std::uint32_t s0 = ent.seq.load(std::memory_order_relaxed);
      ent.seq.store(s0 | 1u, std::memory_order_release);  // begin (odd)
      ent.mv.store(pack_move(best), std::memory_order_relaxed);
      ent.payload.store(pack_payload(value, depth, bound, curAge), std::memory_order_relaxed);
      ent.key.store(key, std::memory_order_release);  // publish key last (für Neu-Eintrag)
      ent.seq.store((s0 | 1u) + 1u, std::memory_order_release);  // end (even)
    };

    const auto write_entry_update = [&](PackedEntry& ent) {
      // Update existierender Key: Key bleibt gleich, Seqlock schützt (payload,mv)
      const std::uint32_t s0 = ent.seq.load(std::memory_order_relaxed);
      ent.seq.store(s0 | 1u, std::memory_order_release);  // begin (odd)
      ent.mv.store(pack_move(best), std::memory_order_relaxed);
      ent.payload.store(pack_payload(value, depth, bound, curAge), std::memory_order_relaxed);
      ent.seq.store((s0 | 1u) + 1u, std::memory_order_release);  // end (even)
    };

    // 1) Update existing
    for (auto& ent : c.e) {
      if (ent.key.load(std::memory_order_acquire) == key) {
        write_entry_update(ent);
        return;
      }
    }

    // 2) Empty slot
    for (auto& ent : c.e) {
      if (ent.key.load(std::memory_order_acquire) == 0) {
        write_entry_newkey(ent);
        return;
      }
    }

    // 3) Replacement (Depth bevorzugen, danach Alter)
    int victim = 0;
    int scoreV = replacement_score(c.e[0], curAge);
    for (int i = 1; i < 4; ++i) {
      const int s = replacement_score(c.e[i], curAge);
      if (s < scoreV) {
        scoreV = s;
        victim = i;
      }
    }
    write_entry_newkey(c.e[victim]);
  }

  void new_generation() {
    auto g = generation_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (g == 0) generation_.store(1, std::memory_order_relaxed);
  }

 private:
  // — Helpers —
  static inline std::uint64_t pack_payload(int32_t value, int16_t depth, Bound bound,
                                           std::uint8_t age) {
    std::uint64_t p = static_cast<std::uint32_t>(value);
    p |= (static_cast<std::uint64_t>(static_cast<std::uint16_t>(depth)) << 32);
    p |= (static_cast<std::uint64_t>(static_cast<std::uint8_t>(bound)) << 48);
    p |= (static_cast<std::uint64_t>(age) << 56);
    return p;
  }
  static inline void unpack_payload(std::uint64_t p, int32_t& value, int16_t& depth, Bound& bound,
                                    std::uint8_t& age) {
    value = static_cast<int32_t>(p & 0xffffffffULL);
    depth = static_cast<int16_t>((p >> 32) & 0xffffULL);
    bound = static_cast<Bound>((p >> 48) & 0xffULL);
    age = static_cast<std::uint8_t>((p >> 56) & 0xffULL);
  }

  // pack: [0..5]=from, [6..11]=to, [12..15]=promo, [16]=isCapture, [17]=isEP, [18..19]=castle
  static inline std::uint32_t pack_move(const Move& m) {
    std::uint32_t v = 0;
    v |= (static_cast<std::uint32_t>(m.from) & 0x3f);
    v |= (static_cast<std::uint32_t>(m.to) & 0x3f) << 6;
    v |= (static_cast<std::uint32_t>(m.promotion) & 0x0f) << 12;
    v |= (m.isCapture ? 1u : 0u) << 16;
    v |= (m.isEnPassant ? 1u : 0u) << 17;
    // castle: 0=None, 1=KingSide, 2=QueenSide
    std::uint32_t c = 0;
    if (m.castle == CastleSide::KingSide)
      c = 1;
    else if (m.castle == CastleSide::QueenSide)
      c = 2;
    v |= (c & 0x3u) << 18;
    return v;
  }

  static inline Move unpack_move(std::uint32_t v) {
    Move m{};
    m.from = static_cast<core::Square>(v & 0x3f);
    m.to = static_cast<core::Square>((v >> 6) & 0x3f);
    m.promotion = static_cast<core::PieceType>((v >> 12) & 0x0f);
    m.isCapture = ((v >> 16) & 1u) != 0;
    m.isEnPassant = ((v >> 17) & 1u) != 0;
    std::uint32_t c = (v >> 18) & 0x3u;
    m.castle =
        (c == 1 ? CastleSide::KingSide : (c == 2 ? CastleSide::QueenSide : CastleSide::None));
    return m;
  }

  static inline int replacement_score(const PackedEntry& ent, std::uint8_t curAge) {
    const std::uint64_t k = ent.key.load(std::memory_order_relaxed);
    if (k == 0) return -1;  // leer -> Topkandidat

    const std::uint64_t pay = ent.payload.load(std::memory_order_relaxed);
    if (pay == 0) return -1;

    int32_t v;
    int16_t d;
    Bound b;
    std::uint8_t age;
    unpack_payload(pay, v, d, b, age);

    // älter = schlechter; tiefer = besser (negativer Score = eher Opfer)
    const int ageDelta = static_cast<std::uint8_t>(curAge - age);
    return static_cast<int>(d) * 256 - ageDelta;
  }

  inline std::size_t index(std::uint64_t key) const {
    assert((slots_ & (slots_ - 1)) == 0 && slots_ != 0);
    return static_cast<std::size_t>(key) & (slots_ - 1);
  }

  static std::size_t highest_power_of_two(std::size_t x) {
    if (x == 0) return 1;
    std::size_t p = 1;
    while ((p << 1) && ((p << 1) <= x)) p <<= 1;
    return p;
  }

  std::unique_ptr<Cluster[]> table_;
  std::size_t slots_ = 1;
  std::atomic<std::uint32_t> generation_{1};
};

}  // namespace lilia::model
