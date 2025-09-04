// include/lilia/model/tt5.hpp
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

#include "move.hpp"  // expects lilia::model::Move etc.

namespace lilia::model {

// -----------------------------------------------------------------------------
// Public entry (for callers)
// -----------------------------------------------------------------------------
enum class Bound : std::uint8_t { Exact = 0, Lower = 1, Upper = 2 };

struct TTEntry5 {
  std::uint64_t key = 0;
  int32_t value = 0;  // score (cp), i16 stored, sign-extended on read
  int16_t depth = 0;  // plies (0..255 stored)
  Bound bound = Bound::Exact;
  Move best{};                                               // move16 packed internally
  std::uint8_t age = 0;                                      // generation (mod 256)
  int16_t staticEval = std::numeric_limits<int16_t>::min();  // INT16_MIN == "unset"
};

// -----------------------------------------------------------------------------
// Tunables / platform helpers
// -----------------------------------------------------------------------------
#if defined(__GNUC__) || defined(__clang__)
#define LILIA_LIKELY(x) (__builtin_expect(!!(x), 1))
#define LILIA_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#define LILIA_PREFETCH_L1(ptr) __builtin_prefetch((ptr), 0, 3)
#define LILIA_PREFETCHW_L1(ptr) __builtin_prefetch((ptr), 1, 3)
#else
#define LILIA_LIKELY(x) (x)
#define LILIA_UNLIKELY(x) (x)
#define LILIA_PREFETCH_L1(ptr) ((void)0)
#define LILIA_PREFETCHW_L1(ptr) ((void)0)
#endif

#ifndef TT5_INDEX_MIX
// 0: use low bits of Zobrist (klassisch), 1: simple mixer (xor-shift)
#define TT5_INDEX_MIX 0
#endif

// -----------------------------------------------------------------------------
// Internal packed entry & cluster
// info bit layout (low -> high):
//  [ 0..15] keyLow16
//  [16..23] age8
//  [24..31] depth8 (plies, clipped 0..255)
//  [32..33] bound2 (0..2)
//  [34..49] keyHigh16
//  [50..62] reserved
//  [63   ] VALID bit (1 = occupied)
// data layout:
//  [ 0..15] move16 (from6|to6|promo4)
//  [16..31] value16 (signed)
//  [32..47] staticEval16 (signed)
//  [48..63] keyHigh16 (redundant; left for compat/diagnostics)
// -----------------------------------------------------------------------------
struct TTEntryPacked {
  std::atomic<std::uint64_t> info{0};
  std::atomic<std::uint64_t> data{0};
  TTEntryPacked() = default;
  TTEntryPacked(const TTEntryPacked&) = delete;
  TTEntryPacked& operator=(const TTEntryPacked&) = delete;
};

struct alignas(64) Cluster {
  std::array<TTEntryPacked, 4> e{};
  Cluster() = default;
  Cluster(const Cluster&) = delete;
  Cluster& operator=(const Cluster&) = delete;
};

// -----------------------------------------------------------------------------
// TT5
// -----------------------------------------------------------------------------
class TT5 {
 public:
  explicit TT5(std::size_t mb = 16) { resize(mb); }

  void resize(std::size_t mb) {
    std::size_t bytes = mb * 1024ULL * 1024ULL;
    if (bytes < sizeof(Cluster)) bytes = sizeof(Cluster);
    std::size_t req = bytes / sizeof(Cluster);
    slots_ = highest_pow2(req);
    if (slots_ == 0) slots_ = 1;

    table_.reset();
    table_ = std::unique_ptr<Cluster[]>(new Cluster[slots_]);
    generation_.store(1u, std::memory_order_relaxed);
  }

  void clear() {
    const auto n = slots_;
    table_.reset();
    table_ = std::unique_ptr<Cluster[]>(new Cluster[n]);
    generation_.store(1u, std::memory_order_relaxed);
  }

  inline void new_generation() noexcept {
    auto g = generation_.fetch_add(1u, std::memory_order_relaxed) + 1u;
    if (g == 0u) generation_.store(1u, std::memory_order_relaxed);
  }

  inline void prefetch(std::uint64_t key) const noexcept { LILIA_PREFETCH_L1(&table_[index(key)]); }

  // --- Probe into user entry ---
  bool probe_into(std::uint64_t key, TTEntry5& out) const noexcept {
    const Cluster& c = table_[index(key)];
    LILIA_PREFETCH_L1(&c);

    const std::uint16_t keyLo = static_cast<std::uint16_t>(key);
    const std::uint16_t keyHi = static_cast<std::uint16_t>(key >> 48);

    for (const auto& ent : c.e) {
      const std::uint64_t info1 = ent.info.load(std::memory_order_acquire);

      // empty?
      if (LILIA_UNLIKELY((info1 & INFO_VALID_MASK) == 0ull)) continue;

      // key low fast-reject
      if (LILIA_UNLIKELY((info1 & INFO_KEYLO_MASK) != keyLo)) continue;

      // key high fast-reject (from info)
      const std::uint16_t infoKeyHi =
          static_cast<std::uint16_t>((info1 >> INFO_KEYHI_SHIFT) & 0xFFFFu);
      if (LILIA_UNLIKELY(infoKeyHi != keyHi)) continue;

      // Ok: read data relaxed
      const std::uint64_t d = ent.data.load(std::memory_order_relaxed);
      // Torn-read/ABA-Schutz: verifiziere KeyHigh auch aus den Daten
      const std::uint16_t dKeyHi = static_cast<std::uint16_t>(d >> 48);
      if (LILIA_UNLIKELY(dKeyHi != keyHi)) continue;

      TTEntry5 tmp{};
      tmp.key = key;
      tmp.age = static_cast<std::uint8_t>((info1 >> INFO_AGE_SHIFT) & 0xFFu);
      tmp.depth = static_cast<int16_t>((info1 >> INFO_DEPTH_SHIFT) & 0xFFu);
      tmp.bound = static_cast<Bound>((info1 >> INFO_BOUND_SHIFT) & 0x3u);

      const std::uint16_t mv16 = static_cast<std::uint16_t>(d & 0xFFFFu);
      tmp.best = unpack_move16(mv16);
      tmp.value = static_cast<int16_t>((d >> 16) & 0xFFFFu);
      tmp.staticEval = static_cast<int16_t>((d >> 32) & 0xFFFFu);

      out = tmp;
      return true;
    }
    return false;
  }

  std::optional<TTEntry5> probe(std::uint64_t key) const {
    TTEntry5 tmp{};
    if (probe_into(key, tmp)) return tmp;
    return std::nullopt;
  }

  // --- Store ---
  void store(std::uint64_t key, int32_t value, int16_t depth, Bound bound, const Move& best,
             int16_t staticEval = std::numeric_limits<int16_t>::min()) noexcept {
    Cluster& c = table_[index(key)];
    LILIA_PREFETCHW_L1(&c);

    const std::uint8_t age = static_cast<std::uint8_t>(generation_.load(std::memory_order_relaxed));
    const std::uint16_t keyLo = static_cast<std::uint16_t>(key);
    const std::uint16_t keyHi = static_cast<std::uint16_t>(key >> 48);

    const std::uint8_t depth8 =
        static_cast<std::uint8_t>(depth < 0 ? 0 : (depth > 255 ? 255 : depth));
    const std::int16_t v16 = static_cast<std::int16_t>(
        value < std::numeric_limits<int16_t>::min()   ? std::numeric_limits<int16_t>::min()
        : value > std::numeric_limits<int16_t>::max() ? std::numeric_limits<int16_t>::max()
                                                      : value);
    const std::int16_t se16 = static_cast<std::int16_t>(
        std::clamp(staticEval, std::numeric_limits<int16_t>::min(),
                   std::numeric_limits<int16_t>::max()));
    const std::uint16_t mv16 = pack_move16(best);

    // 1) Update same key if present (no data read needed)
    for (auto& ent : c.e) {
      const std::uint64_t info1 = ent.info.load(std::memory_order_acquire);

      if ((info1 & INFO_VALID_MASK) == 0ull) continue;
      if ((info1 & INFO_KEYLO_MASK) != keyLo) continue;

      const std::uint16_t infoKeyHi =
          static_cast<std::uint16_t>((info1 >> INFO_KEYHI_SHIFT) & 0xFFFFu);
      if (infoKeyHi != keyHi) continue;

      const std::uint8_t od = static_cast<std::uint8_t>((info1 >> INFO_DEPTH_SHIFT) & 0xFFu);
      const Bound ob = static_cast<Bound>((info1 >> INFO_BOUND_SHIFT) & 0x3u);

      // conservative policy: keep stronger bounds/deeper
      const unsigned oldD = od;
      const unsigned newD = depth8;
      bool replace = true;
      if (bound == Bound::Upper && (ob == Bound::Exact || ob == Bound::Lower) && oldD > newD) {
        replace = false;
      } else if (bound != Bound::Exact) {
        if (oldD > newD + 1u && ob != Bound::Upper) replace = false;
      }

      if (!replace) return;

      const std::uint64_t newData =
          (static_cast<std::uint64_t>(mv16) & 0xFFFFull) |
          (static_cast<std::uint64_t>(static_cast<std::uint16_t>(v16)) << 16) |
          (static_cast<std::uint64_t>(static_cast<std::uint16_t>(se16)) << 32) |
          (static_cast<std::uint64_t>(keyHi) << 48);

      const std::uint64_t newInfo =
          INFO_VALID_MASK | (static_cast<std::uint64_t>(keyLo)) |
          (static_cast<std::uint64_t>(age) << INFO_AGE_SHIFT) |
          (static_cast<std::uint64_t>(depth8) << INFO_DEPTH_SHIFT) |
          (static_cast<std::uint64_t>(static_cast<std::uint8_t>(bound)) << INFO_BOUND_SHIFT) |
          (static_cast<std::uint64_t>(keyHi) << INFO_KEYHI_SHIFT);

      ent.data.store(newData, std::memory_order_relaxed);
      ent.info.store(newInfo, std::memory_order_release);
      return;
    }

    // 2) Free slot (info VALID bit == 0)
    for (auto& ent : c.e) {
      const std::uint64_t info1 = ent.info.load(std::memory_order_relaxed);
      if ((info1 & INFO_VALID_MASK) == 0ull) {
        write_entry(ent, keyLo, keyHi, age, depth8, bound, mv16, v16, se16);
        return;
      }
    }

    // 3) Replacement: choose victim by heuristic
    int victim = 0;
    int bestScore = repl_score(c.e[0], age);
    for (int i = 1; i < 4; ++i) {
      const int sc = repl_score(c.e[i], age);
      if (sc < bestScore) {
        bestScore = sc;
        victim = i;
      }
    }
    write_entry(c.e[victim], keyLo, keyHi, age, depth8, bound, mv16, v16, se16);
  }

 private:
  // --- bitfield constants ---
  static constexpr std::uint64_t INFO_KEYLO_MASK = 0xFFFFull;
  static constexpr unsigned INFO_AGE_SHIFT = 16;
  static constexpr unsigned INFO_DEPTH_SHIFT = 24;
  static constexpr unsigned INFO_BOUND_SHIFT = 32;
  static constexpr unsigned INFO_KEYHI_SHIFT = 34;
  static constexpr std::uint64_t INFO_VALID_MASK = (1ull << 63);

  // --- move packing (16 bit) ---
  static inline std::uint16_t pack_move16(const Move& m) noexcept {
    const std::uint16_t from = static_cast<std::uint16_t>(static_cast<unsigned>(m.from) & 0x3F);
    const std::uint16_t to = static_cast<std::uint16_t>(static_cast<unsigned>(m.to) & 0x3F);
    const std::uint16_t promo =
        static_cast<std::uint16_t>(static_cast<unsigned>(m.promotion) & 0x0F);
    return static_cast<std::uint16_t>(from | (to << 6) | (promo << 12));
  }
  static inline Move unpack_move16(std::uint16_t v) noexcept {
    Move m{};
    m.from = static_cast<core::Square>(v & 0x3F);
    m.to = static_cast<core::Square>((v >> 6) & 0x3F);
    m.promotion = static_cast<core::PieceType>((v >> 12) & 0x0F);
    m.isCapture = false;
    m.isEnPassant = false;
    m.castle = CastleSide::None;
    return m;
  }

  // --- replacement score: lower is worse (chosen as victim) ---
  static inline int repl_score(const TTEntryPacked& ent, std::uint8_t curAge) noexcept {
    const std::uint64_t info = ent.info.load(std::memory_order_relaxed);
    if ((info & INFO_VALID_MASK) == 0ull)
      return std::numeric_limits<int>::min();  // empty → best victim

    const std::uint8_t age = static_cast<std::uint8_t>((info >> INFO_AGE_SHIFT) & 0xFFu);
    const std::uint8_t dep = static_cast<std::uint8_t>((info >> INFO_DEPTH_SHIFT) & 0xFFu);
    const Bound bnd = static_cast<Bound>((info >> INFO_BOUND_SHIFT) & 0x3u);

    const int boundBias = (bnd == Bound::Exact ? 6 : (bnd == Bound::Lower ? 3 : 0));
    const int ageDelta = static_cast<std::uint8_t>(curAge - age);  // modulo 256

    // Depth dominates, then bound quality, then freshness
    return static_cast<int>(dep) * 256 + boundBias - ageDelta;
  }

  static inline void write_entry(TTEntryPacked& ent, std::uint16_t keyLo, std::uint16_t keyHi,
                                 std::uint8_t age, std::uint8_t depth8, Bound bound,
                                 std::uint16_t mv16, std::int16_t v16, std::int16_t se16) noexcept {
    const std::uint64_t newData =
        (static_cast<std::uint64_t>(mv16) & 0xFFFFull) |
        (static_cast<std::uint64_t>(static_cast<std::uint16_t>(v16)) << 16) |
        (static_cast<std::uint64_t>(static_cast<std::uint16_t>(se16)) << 32) |
        (static_cast<std::uint64_t>(keyHi) << 48);

    const std::uint64_t newInfo =
        INFO_VALID_MASK | (static_cast<std::uint64_t>(keyLo)) |
        (static_cast<std::uint64_t>(age) << INFO_AGE_SHIFT) |
        (static_cast<std::uint64_t>(depth8) << INFO_DEPTH_SHIFT) |
        (static_cast<std::uint64_t>(static_cast<std::uint8_t>(bound)) << INFO_BOUND_SHIFT) |
        (static_cast<std::uint64_t>(keyHi) << INFO_KEYHI_SHIFT);

    ent.data.store(newData, std::memory_order_relaxed);
    ent.info.store(newInfo, std::memory_order_release);
  }

  inline std::size_t index(std::uint64_t key) const noexcept {
    // slots_ is power-of-two
#if TT5_INDEX_MIX
    // simple mixer using both halves + shift (helps if low bits are biased)
    std::uint64_t h = key ^ (key >> 32) ^ (key << 13);
    return static_cast<std::size_t>(h) & (slots_ - 1);
#else
    return static_cast<std::size_t>(key) & (slots_ - 1);
#endif
  }

  static inline std::size_t highest_pow2(std::size_t x) noexcept {
    if (x == 0) return 1;
#if defined(__GNUC__) || defined(__clang__)
    // next lower power of two
    int lz = __builtin_clzll(static_cast<unsigned long long>(x));
    std::size_t p = 1ull << (63 - lz);
    if (p > x) p >>= 1;
    return p ? p : 1;
#else
    // portable fallback
    std::size_t p = 1;
    while ((p << 1) && ((p << 1) <= x)) p <<= 1;
    return p;
#endif
  }

  std::unique_ptr<Cluster[]> table_;
  std::size_t slots_ = 1;
  std::atomic<std::uint32_t> generation_{1u};
};

}  // namespace lilia::model
