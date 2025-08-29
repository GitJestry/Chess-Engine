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

enum class Bound : std::uint8_t { Exact = 0, Lower = 1, Upper = 2 };

struct TTEntry4 {
  std::uint64_t key = 0;
  int32_t value = 0;
  int16_t depth = 0;
  Bound bound = Bound::Exact;
  Move best{};
  std::uint8_t age = 0;
};

// --------- Internals ---------

struct PackedEntry {
  std::atomic<std::uint64_t> key{0};      // publish-last on new writes
  std::atomic<std::uint64_t> payload{0};  // value/depth/bound/age
  std::atomic<std::uint32_t> mv{0};       // packed move
  std::atomic<std::uint32_t> seq{0};      // seqlock: even=stable, odd=writer
  PackedEntry() = default;
  PackedEntry(const PackedEntry&) = delete;
  PackedEntry& operator=(const PackedEntry&) = delete;
  PackedEntry(PackedEntry&&) = delete;
  PackedEntry& operator=(PackedEntry&&) = delete;
};

struct alignas(64) Cluster {
  std::array<PackedEntry, 4> e{};  // stride wird 128B (Padding), cachefreundlich
  Cluster() = default;
  Cluster(const Cluster&) = delete;
  Cluster& operator=(const Cluster&) = delete;
  Cluster(Cluster&&) = delete;
  Cluster& operator=(Cluster&&) = delete;
};

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
    const auto oldSlots = slots_;
    table_.reset();
    table_ = std::unique_ptr<Cluster[]>(new Cluster[oldSlots]);
    generation_.store(1, std::memory_order_relaxed);
  }

  // --------- Probe (hot path) ----------
  // Liefert true auf Hit und füllt 'out' konsistent (Seqlock-gesichert).
  bool probe_into(std::uint64_t key, TTEntry4& out) const noexcept {
    const Cluster& c = table_[index(key)];
    LILIA_PREFETCH_L1(&c);

    for (const auto& ent : c.e) {
      const std::uint64_t k1 = ent.key.load(std::memory_order_relaxed);
      if (k1 != key) continue;

      for (int tries = 0; tries < 3; ++tries) {
        const std::uint32_t s1 = ent.seq.load(std::memory_order_acquire);
        if (s1 & 1u) continue;  // writer active

        const std::uint64_t pay = ent.payload.load(std::memory_order_relaxed);
        const std::uint32_t mv = ent.mv.load(std::memory_order_relaxed);

        const std::uint32_t s2 = ent.seq.load(std::memory_order_acquire);
        if ((s2 != s1) || (s2 & 1u)) continue;

        out.key = key;
        unpack_payload(pay, out.value, out.depth, out.bound, out.age);
        out.best = unpack_move(mv);
        return true;
      }
    }
    return false;
  }

  // optional, kompatibel
  std::optional<TTEntry4> probe(std::uint64_t key) const {
    TTEntry4 tmp{};
    if (probe_into(key, tmp)) return tmp;
    return std::nullopt;
  }

  // --------- Store (thread-safe, seqlock) ----------
  void store(std::uint64_t key, int32_t value, int16_t depth, Bound bound,
             const Move& best) noexcept {
    Cluster& c = table_[index(key)];
    const std::uint8_t curAge =
        static_cast<std::uint8_t>(generation_.load(std::memory_order_relaxed));

    // Robustheit: clamp Depth/Value in Feldbreite
    if (depth < INT16_C(-32768)) depth = INT16_C(-32768);
    if (depth > INT16_C(32767)) depth = INT16_C(32767);
    // (value trennt sich in encode/decoder → hier keine Mate-Norm nötig)

    const auto packPay = [&](int32_t v, int16_t d, Bound b, std::uint8_t a) {
      return pack_payload(v, d, b, a);
    };

    const auto write_newkey = [&](PackedEntry& ent, int32_t v, int16_t d, Bound b,
                                  const Move& m) noexcept {
      const std::uint32_t s0 = ent.seq.load(std::memory_order_relaxed);
      ent.seq.store(s0 | 1u, std::memory_order_release);  // begin (odd)

      ent.mv.store(pack_move(m), std::memory_order_relaxed);
      ent.payload.store(packPay(v, d, b, curAge), std::memory_order_relaxed);

      ent.key.store(key, std::memory_order_release);             // publish key last (new entry)
      ent.seq.store((s0 | 1u) + 1u, std::memory_order_release);  // end (even)
    };

    const auto write_update = [&](PackedEntry& ent, int32_t v, int16_t d, Bound b,
                                  const Move& m) noexcept {
      const std::uint32_t s0 = ent.seq.load(std::memory_order_relaxed);
      ent.seq.store(s0 | 1u, std::memory_order_release);  // begin (odd)

      ent.mv.store(pack_move(m), std::memory_order_relaxed);
      ent.payload.store(packPay(v, d, b, curAge), std::memory_order_relaxed);

      ent.seq.store((s0 | 1u) + 1u, std::memory_order_release);  // end (even)
    };

    // 1) Update existierender Key – aber nur, wenn sinnvoll (write-throttling).
    for (auto& ent : c.e) {
      if (ent.key.load(std::memory_order_relaxed) == key) {
        // alten Payload stabil lesen (kurz, best effort)
        int32_t ov = 0;
        int16_t od = 0;
        Bound ob = Bound::Exact;
        std::uint8_t oa = 0;
        bool have_old = false;
        for (int tries = 0; tries < 3; ++tries) {
          const std::uint32_t s1 = ent.seq.load(std::memory_order_acquire);
          if (s1 & 1u) continue;
          const std::uint64_t pay = ent.payload.load(std::memory_order_relaxed);
          const std::uint32_t s2 = ent.seq.load(std::memory_order_acquire);
          if (s1 == s2 && !(s2 & 1u)) {
            unpack_payload(pay, ov, od, ob, oa);
            have_old = true;
            break;
          }
        }

        // Politik:
        // - Exact immer >= (ersetzt alles)
        // - Lower ersetzt Upper, wenn Tiefe >= altDepth - 1
        // - Upper ersetzt gar nichts tieferes (spart Writes)
        // - Höhere Tiefe ersetzt gleiche/geringere Tiefe
        bool replace = true;
        if (have_old) {
          if (bound == Bound::Upper && (ob == Bound::Exact || ob == Bound::Lower) && od > depth)
            replace = false;
          else if (bound != Bound::Exact) {
            if (depth + 1 < od && ob != Bound::Upper) replace = false;
          }
        }

        if (replace) write_update(ent, value, depth, bound, best);
        // sonst: kein Age-Refresh, um unnötige Writes zu sparen
        return;
      }
    }

    // 2) Leer-Slot
    for (auto& ent : c.e) {
      if (ent.key.load(std::memory_order_relaxed) == 0ULL) {
        write_newkey(ent, value, depth, bound, best);
        return;
      }
    }

    // 3) Replacement: wähle Opfer nach „Depth >> Bound >> (Jünger)“
    int victim = 0;
    int scoreV = replacement_score(c.e[0], curAge);
    for (int i = 1; i < 4; ++i) {
      const int s = replacement_score(c.e[i], curAge);
      if (s < scoreV) {
        scoreV = s;
        victim = i;
      }
    }
    write_newkey(c.e[victim], value, depth, bound, best);
  }

  void new_generation() noexcept {
    auto g = generation_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (g == 0) generation_.store(1, std::memory_order_relaxed);
  }

  void prefetch(std::uint64_t key) const { LILIA_PREFETCH_L1(&table_[index(key)]); }

 private:
  // pack: [0..31]=value, [32..47]=depth(u16), [48..55]=bound(u8), [56..63]=age(u8)
  static inline std::uint64_t pack_payload(int32_t value, int16_t depth, Bound bound,
                                           std::uint8_t age) noexcept {
    std::uint64_t p = static_cast<std::uint32_t>(value);
    p |= (static_cast<std::uint64_t>(static_cast<std::uint16_t>(depth)) << 32);
    p |= (static_cast<std::uint64_t>(static_cast<std::uint8_t>(bound)) << 48);
    p |= (static_cast<std::uint64_t>(age) << 56);
    return p;
  }

  static inline void unpack_payload(std::uint64_t p, int32_t& value, int16_t& depth, Bound& bound,
                                    std::uint8_t& age) noexcept {
    value = static_cast<int32_t>(p & 0xffffffffULL);
    depth = static_cast<int16_t>((p >> 32) & 0xffffULL);
    bound = static_cast<Bound>((p >> 48) & 0xffULL);
    age = static_cast<std::uint8_t>((p >> 56) & 0xffULL);
  }

  // Move packen: [0..5]=from, [6..11]=to, [12..15]=promo, [16]=cap, [17]=ep, [18..19]=castle
  static inline std::uint32_t pack_move(const Move& m) noexcept {
    std::uint32_t v = 0;
    v |= (static_cast<std::uint32_t>(m.from) & 0x3f);
    v |= (static_cast<std::uint32_t>(m.to) & 0x3f) << 6;
    v |= (static_cast<std::uint32_t>(m.promotion) & 0x0f) << 12;
    v |= (m.isCapture ? 1u : 0u) << 16;
    v |= (m.isEnPassant ? 1u : 0u) << 17;
    std::uint32_t c = 0;
    if (m.castle == CastleSide::KingSide)
      c = 1;
    else if (m.castle == CastleSide::QueenSide)
      c = 2;
    v |= (c & 0x3u) << 18;
    return v;
  }

  static inline Move unpack_move(std::uint32_t v) noexcept {
    Move m{};
    m.from = static_cast<core::Square>(v & 0x3f);
    m.to = static_cast<core::Square>((v >> 6) & 0x3f);
    m.promotion = static_cast<core::PieceType>((v >> 12) & 0x0f);
    m.isCapture = ((v >> 16) & 1u) != 0;
    m.isEnPassant = ((v >> 17) & 1u) != 0;
    const std::uint32_t c = (v >> 18) & 0x3u;
    m.castle =
        (c == 1 ? CastleSide::KingSide : (c == 2 ? CastleSide::QueenSide : CastleSide::None));
    return m;
  }

  // Replacement score: größere Werte = „behalten“. Opfer ist das Minimum.
  static inline int replacement_score(const PackedEntry& ent, std::uint8_t curAge) noexcept {
    const std::uint64_t k = ent.key.load(std::memory_order_relaxed);
    if (k == 0ULL) return INT_MIN;  // leerer Slot = bester Kandidat (sofortiger Ersatz)

    // ungeschützt lesen ist ok – heuristisch. Seqlock hier nicht nötig.
    const std::uint64_t pay = ent.payload.load(std::memory_order_relaxed);

    int32_t v;
    (void)v;
    int16_t d;
    Bound b;
    std::uint8_t a;
    unpack_payload(pay, v, d, b, a);

    const int boundBias = (b == Bound::Exact ? 6 : (b == Bound::Lower ? 3 : 0));
    const int ageDelta = static_cast<std::uint8_t>(curAge - a);  // 8-bit wrap

    // Tiefer ist besser, Exact besser, jünger besser.
    return static_cast<int>(d) * 256 + boundBias - ageDelta;
  }

  inline std::size_t index(std::uint64_t key) const noexcept {
    assert((slots_ & (slots_ - 1)) == 0 && slots_ != 0);
    return static_cast<std::size_t>(key) & (slots_ - 1);
  }

  static std::size_t highest_power_of_two(std::size_t x) noexcept {
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
