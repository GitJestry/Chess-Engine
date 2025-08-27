#include "lilia/engine/eval.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <limits>
#include <mutex>

#include "lilia/model/core/bitboard.hpp"
#include "lilia/model/core/magic.hpp"
#include "lilia/model/position.hpp"

using namespace lilia::core;
using namespace lilia::model;
using namespace lilia::model::bb;

namespace lilia::engine {

constexpr int INF = std::numeric_limits<int>::max() / 4;
constexpr Bitboard CENTER_MASK =
    sq_bb(Square(27)) | sq_bb(Square(28)) | sq_bb(Square(35)) | sq_bb(Square(36));

constexpr std::array<int, 6> PIECE_VALUE_MG = {100, 320, 330, 500, 900, 0};
constexpr std::array<int, 6> PIECE_VALUE_EG = {100, 320, 330, 500, 900, 0};
constexpr std::array<int, 6> PIECE_PHASE = {0, 1, 1, 2, 4, 0};

constexpr int BISHOP_PAIR_BONUS = 44;
constexpr int ROOK_OPEN_FILE_BONUS = 18;
constexpr int ROOK_SEMI_OPEN_FILE_BONUS = 9;
constexpr int ROOK_ON_SEVENTH_BONUS = 30;
constexpr int KNIGHT_RIM_PENALTY = 15;
constexpr int DEVELOPMENT_PENALTY = 20;
constexpr int OUTPOST_KNIGHT_BONUS = 30;
constexpr int CENTER_CONTROL_BONUS = 6;
constexpr int CONNECTED_ROOKS_BONUS = 20;
constexpr int TEMPO_BONUS = 12;

// ---------- Helpers ----------
inline constexpr int mirror_sq_black(int sq) noexcept {
  return sq ^ 56;
}  // rank flip

// ===== PSTs auf deine Eval zugeschnitten =====

// ---------- Pawn ----------
static constexpr std::array<int, 64> PST_P_MG = {
    0,  0,  0,  0,  0,  0,  0,  0,  5,  6,  2,  -6, -6, 2,  6,  5,  3,  -3, -4, 2,  2,  -4,
    -3, 3,  5,  7,  10, 14, 14, 10, 7,  5,  9,  12, 16, 21, 21, 16, 12, 9,  13, 18, 22, 26,
    26, 22, 18, 13, 18, 18, 18, 18, 18, 18, 18, 18, 0,  0,  0,  0,  0,  0,  0,  0};
static constexpr std::array<int, 64> PST_P_EG = {
    0,  0,  0,  0,  0,  0,  0,  0,  6,  8,  4,  -2, -2, 4,  8,  6,  5,  2,  0,  6,  6,  0,
    2,  5,  8,  10, 13, 18, 18, 13, 10, 8,  13, 18, 22, 28, 28, 22, 18, 13, 18, 24, 30, 38,
    38, 30, 24, 18, 22, 30, 38, 46, 46, 38, 30, 22, 0,  0,  0,  0,  0,  0,  0,  0};

// ---------- Knight ----------
static constexpr std::array<int, 64> PST_N_MG = {
    -48, -38, -28, -24, -24, -28, -38, -48, -36, -18, -4,  0,   0,   -4,  -18, -36,
    -26, -4,  10,  16,  16,  10,  -4,  -26, -22, 0,   16,  24,  24,  16,  0,   -22,
    -22, 0,   16,  24,  24,  16,  0,   -22, -26, -4,  10,  16,  16,  10,  -4,  -26,
    -36, -16, -2,  0,   0,   -2,  -16, -36, -48, -38, -30, -26, -26, -30, -38, -48};
static constexpr std::array<int, 64> PST_N_EG = {
    -38, -28, -20, -16, -16, -20, -28, -38, -28, -12, -2,  4,   4,   -2,  -12, -28,
    -20, -2,  9,   14,  14,  9,   -2,  -20, -16, 4,   14,  22,  22,  14,  4,   -16,
    -16, 4,   14,  22,  22,  14,  4,   -16, -20, -2,  9,   14,  14,  9,   -2,  -20,
    -28, -12, -2,  4,   4,   -2,  -12, -28, -38, -28, -20, -16, -16, -20, -28, -38};

// ---------- Bishop ----------
static constexpr std::array<int, 64> PST_B_MG = {
    -28, -16, -12, -9, -9, -12, -16, -28, -14, -5,  2,   5,  5,  2,   -5,  -14,
    -10, 2,   9,   13, 13, 9,   2,   -10, -7,  5,   13,  18, 18, 13,  5,   -7,
    -7,  5,   13,  18, 18, 13,  5,   -7,  -10, 2,   9,   13, 13, 9,   2,   -10,
    -14, -5,  2,   5,  5,  2,   -5,  -14, -26, -14, -10, -7, -7, -10, -14, -26};
static constexpr std::array<int, 64> PST_B_EG = {
    -20, -10, -6, -2, -2,  -6, -10, -20, -10, -2, 6,  9,   9,   6,  -2, -10, -6, 6,  12, 16, 16, 12,
    6,   -6,  -2, 9,  16,  22, 22,  16,  9,   -2, -2, 9,   16,  22, 22, 16,  9,  -2, -6, 6,  12, 16,
    16,  12,  6,  -6, -10, -2, 6,   9,   9,   6,  -2, -10, -18, -9, -5, -2,  -2, -5, -9, -18};

// ---------- Rook ----------
static constexpr std::array<int, 64> PST_R_MG = {
    0, 2, 2, 4, 4, 2, 2, 0, -2, 0, 1, 3, 3, 1, 0, -2, -3, -1, 0, 2, 2, 0, -1, -3, -4, -1, 1, 2, 2,
    1, -1, -4, -4, -1, 1, 2, 2, 1, -1, -4, -3, -1, 0, 2, 2, 0, -1, -3, 4, 6, 6, 8, 8, 6, 6,
    4,  // leicht aktiv, aber
        // 7th-Boost NICHT hier
    2, 4, 4, 6, 6, 4, 4, 2};
static constexpr std::array<int, 64> PST_R_EG = {
    2, 3,  5,  7,  7, 5, 3, 2, 0, 2,  3,  5, 5, 3, 2, 0,  -1, 1,  2,  4, 4, 2,
    1, -1, -1, 1,  2, 4, 4, 2, 1, -1, -1, 1, 2, 4, 4, 2,  1,  -1, -1, 1, 2, 4,
    4, 2,  1,  -1, 3, 5, 7, 9, 9, 7,  5,  3, 4, 6, 8, 10, 10, 8,  6,  4};

// ---------- Queen ----------
static constexpr std::array<int, 64> PST_Q_MG = {
    -22, -14, -10, -8, -8, -10, -14, -22, -14, -8,  -4,  -2, -2, -4,  -8,  -14,
    -10, -4,  2,   4,  4,  2,   -4,  -10, -8,  -2,  4,   6,  6,  4,   -2,  -8,
    -8,  -2,  4,   6,  6,  4,   -2,  -8,  -10, -4,  2,   4,  4,  2,   -4,  -10,
    -14, -8,  -4,  -2, -2, -4,  -8,  -14, -22, -14, -10, -8, -8, -10, -14, -22};
static constexpr std::array<int, 64> PST_Q_EG = {
    -10, -5, -2, 0,  0,  -2, -5, -10, -5, -2, 2,  4,  4,   2,  -2, -5, -2, 2,  5,  8,  8, 5,
    2,   -2, 0,  4,  8,  11, 11, 8,   4,  0,  0,  4,  8,   11, 11, 8,  4,  0,  -2, 2,  5, 8,
    8,   5,  2,  -2, -5, -2, 2,  4,   4,  2,  -2, -5, -10, -5, -2, 0,  0,  -2, -5, -10};

// ---------- King ----------
static constexpr std::array<int, 64> PST_K_MG = {
    -34, -44, -46, -52, -52, -46, -44, -34, -30, -38, -40, -48, -48, -40, -38, -30,
    -26, -34, -36, -44, -44, -36, -34, -26, -14, -20, -24, -32, -32, -24, -20, -14,
    -2,  -4,  -10, -18, -18, -10, -4,  -2,  8,   12,  -2,  -12, -12, -2,  12,  8,
    18,  24,  12,  0,   0,   12,  24,  18,  26,  36,  26,  10,  10,  26,  36,  26};
static constexpr std::array<int, 64> PST_K_EG = {
    -8, -4, -4, -2, -2, -4, -4, -8, -4, 2,  4,  6,  6,  4,  2,  -4, -4, 4,  10, 12, 12, 10,
    4,  -4, -2, 6,  12, 18, 18, 12, 6,  -2, -2, 6,  12, 18, 18, 12, 6,  -2, -4, 4,  10, 12,
    12, 10, 4,  -4, -4, 2,  4,  6,  6,  4,  2,  -4, -8, -4, -4, -2, -2, -4, -4, -8};

static inline int pst_eg_for(core::PieceType pt, int sq) noexcept {
  switch (pt) {
    case core::PieceType::Pawn:
      return PST_P_EG[sq];
    case core::PieceType::Knight:
      return PST_N_EG[sq];
    case core::PieceType::Bishop:
      return PST_B_EG[sq];
    case core::PieceType::Rook:
      return PST_R_EG[sq];
    case core::PieceType::Queen:
      return PST_Q_EG[sq];
    case core::PieceType::King:
      return PST_K_EG[sq];
    default:
      return 0;
  }
}

static inline int pst_mg_for(core::PieceType pt, int sq) noexcept {
  switch (pt) {
    case core::PieceType::Pawn:
      return PST_P_MG[sq];
    case core::PieceType::Knight:
      return PST_N_MG[sq];
    case core::PieceType::Bishop:
      return PST_B_MG[sq];
    case core::PieceType::Rook:
      return PST_R_MG[sq];
    case core::PieceType::Queen:
      return PST_Q_MG[sq];
    case core::PieceType::King:
      return PST_K_MG[sq];
    default:
      return 0;
  }
}

inline int sq_file(int sq) noexcept {
  return sq & 7;
}
inline int sq_rank(int sq) noexcept {
  return sq >> 3;
}
inline int popcnt(Bitboard b) noexcept {
  return popcount(b);
}
inline int lsb_i(Bitboard b) noexcept {
  return b ? ctz64(b) : -1;
}

struct PrecomputedMasks {
  std::array<Bitboard, 64> passed_white;
  std::array<Bitboard, 64> passed_black;
  std::array<Bitboard, 64> file_mask;
  std::array<Bitboard, 64> adjacent_files;
  std::array<Bitboard, 64> pawn_front_white;
  std::array<Bitboard, 64> pawn_front_black;
};

static PrecomputedMasks masks;
static std::once_flag masks_once;

static void init_masks_if_needed() {
  std::call_once(masks_once, []() {
    for (int sq = 0; sq < 64; ++sq) {
      Bitboard b = sq_bb(static_cast<core::Square>(sq));
      int f = sq_file(sq);
      int r = sq_rank(sq);

      Bitboard fm = 0;
      for (int rr = 0; rr < 8; ++rr) fm |= sq_bb(static_cast<core::Square>((rr << 3) | f));
      masks.file_mask[sq] = fm;

      Bitboard adj = 0;
      if (f > 0)
        for (int rr = 0; rr < 8; ++rr) adj |= sq_bb(static_cast<core::Square>((rr << 3) | (f - 1)));
      if (f < 7)
        for (int rr = 0; rr < 8; ++rr) adj |= sq_bb(static_cast<core::Square>((rr << 3) | (f + 1)));
      masks.adjacent_files[sq] = adj;

      Bitboard p_w = 0;
      for (int rr = r + 1; rr < 8; ++rr)
        for (int ff = std::max(0, f - 1); ff <= std::min(7, f + 1); ++ff)
          p_w |= sq_bb(static_cast<core::Square>((rr << 3) | ff));
      masks.passed_white[sq] = p_w;

      Bitboard p_b = 0;
      for (int rr = r - 1; rr >= 0; --rr)
        for (int ff = std::max(0, f - 1); ff <= std::min(7, f + 1); ++ff)
          p_b |= sq_bb(static_cast<core::Square>((rr << 3) | ff));
      masks.passed_black[sq] = p_b;

      Bitboard span_w = 0;
      for (int rr = r + 1; rr < 8; ++rr) span_w |= sq_bb(static_cast<core::Square>((rr << 3) | f));
      masks.pawn_front_white[sq] = span_w;

      Bitboard span_b = 0;
      for (int rr = r - 1; rr >= 0; --rr) span_b |= sq_bb(static_cast<core::Square>((rr << 3) | f));
      masks.pawn_front_black[sq] = span_b;
    }
  });
}

static void material_pst_phase(const std::array<Bitboard, 6>& wbbs,
                               const std::array<Bitboard, 6>& bbbs, int& mg_out, int& eg_out,
                               int& phase_out) {
  mg_out = eg_out = 0;
  phase_out = 0;

  // Weiß
  for (int pt = 0; pt < 6; ++pt) {
    auto piece = static_cast<PieceType>(pt);
    Bitboard bb = wbbs[pt];
    while (bb) {
      int sq = lsb_i(bb);
      bb &= bb - 1;
      mg_out += PIECE_VALUE_MG[pt] + pst_mg_for(piece, sq);
      eg_out += PIECE_VALUE_EG[pt] + pst_eg_for(piece, sq);
      phase_out += PIECE_PHASE[pt];
    }
  }

  // Schwarz (rank-mirror!)
  for (int pt = 0; pt < 6; ++pt) {
    auto piece = static_cast<PieceType>(pt);
    Bitboard bb = bbbs[pt];
    while (bb) {
      int sq = lsb_i(bb);
      int msq = mirror_sq_black(sq);  // <--- wichtig
      bb &= bb - 1;
      mg_out -= PIECE_VALUE_MG[pt] + pst_mg_for(piece, msq);
      eg_out -= PIECE_VALUE_EG[pt] + pst_eg_for(piece, msq);
      phase_out += PIECE_PHASE[pt];
    }
  }
}

static int pawn_structure(Bitboard wp_orig, Bitboard bp_orig) {
  init_masks_if_needed();
  int score = 0;
  Bitboard wp = wp_orig;
  Bitboard bp = bp_orig;

  while (wp) {
    int sq = lsb_i(wp);
    wp &= wp - 1;
    if ((masks.passed_white[sq] & bp_orig) == 0) {
      int r = sq_rank(sq);      // 0..7
      int bonus = 10 + r * 12;  // skaliert etwas steiler als vorher
      // Blocked penalty: Gegner blockiert das Vorfeld?
      int frontSq = sq + 8;
      if (frontSq <= 63 && (bp_orig & sq_bb(static_cast<core::Square>(frontSq)))) {
        bonus -= 10 + r * 4;  // stärker je weiter vorne
      }
      score += bonus;
    }
  }
  while (bp) {
    int sq = lsb_i(bp);
    bp &= bp - 1;
    if ((masks.passed_black[sq] & wp_orig) == 0) {
      int r = sq_rank(sq);
      int bonus = 10 + (7 - r) * 12;
      int frontSq = sq - 8;
      if (frontSq >= 0 && (wp_orig & sq_bb(static_cast<core::Square>(frontSq)))) {
        bonus -= 10 + (7 - r) * 4;
      }
      score -= bonus;
    }
  }
  return score;
}

static int mobility(Bitboard occ, Bitboard wocc, Bitboard bocc, const std::array<Bitboard, 6>& wbbs,
                    const std::array<Bitboard, 6>& bbbs) {
  int sc = 0;

  Bitboard wn = wbbs[static_cast<int>(PieceType::Knight)];
  while (wn) {
    int sq = lsb_i(wn);
    wn &= wn - 1;
    Bitboard a = knight_attacks_from(static_cast<core::Square>(sq)) & ~wocc;
    sc += (popcnt(a) - 4) * 5;
  }
  Bitboard bn = bbbs[static_cast<int>(PieceType::Knight)];
  while (bn) {
    int sq = lsb_i(bn);
    bn &= bn - 1;
    Bitboard a = knight_attacks_from(static_cast<core::Square>(sq)) & ~bocc;
    sc -= (popcnt(a) - 4) * 5;
  }

  Bitboard wb = wbbs[static_cast<int>(PieceType::Bishop)];
  while (wb) {
    int sq = lsb_i(wb);
    wb &= wb - 1;
    Bitboard a =
        magic::sliding_attacks(magic::Slider::Bishop, static_cast<core::Square>(sq), occ) & ~wocc;
    sc += popcnt(a) * 3;
  }
  Bitboard bb = bbbs[static_cast<int>(PieceType::Bishop)];
  while (bb) {
    int sq = lsb_i(bb);
    bb &= bb - 1;
    Bitboard a =
        magic::sliding_attacks(magic::Slider::Bishop, static_cast<core::Square>(sq), occ) & ~bocc;
    sc -= popcnt(a) * 3;
  }

  Bitboard wr = wbbs[static_cast<int>(PieceType::Rook)];
  while (wr) {
    int sq = lsb_i(wr);
    wr &= wr - 1;
    Bitboard a =
        magic::sliding_attacks(magic::Slider::Rook, static_cast<core::Square>(sq), occ) & ~wocc;
    sc += popcnt(a) * 2;
  }
  Bitboard br = bbbs[static_cast<int>(PieceType::Rook)];
  while (br) {
    int sq = lsb_i(br);
    br &= br - 1;
    Bitboard a =
        magic::sliding_attacks(magic::Slider::Rook, static_cast<core::Square>(sq), occ) & ~bocc;
    sc -= popcnt(a) * 2;
  }

  Bitboard wq = wbbs[static_cast<int>(PieceType::Queen)];
  while (wq) {
    int sq = lsb_i(wq);
    wq &= wq - 1;
    Bitboard a =
        (magic::sliding_attacks(magic::Slider::Rook, static_cast<core::Square>(sq), occ) |
         magic::sliding_attacks(magic::Slider::Bishop, static_cast<core::Square>(sq), occ)) &
        ~wocc;
    sc += popcnt(a) * 1;
  }
  Bitboard bq = bbbs[static_cast<int>(PieceType::Queen)];
  while (bq) {
    int sq = lsb_i(bq);
    bq &= bq - 1;
    Bitboard a =
        (magic::sliding_attacks(magic::Slider::Rook, static_cast<core::Square>(sq), occ) |
         magic::sliding_attacks(magic::Slider::Bishop, static_cast<core::Square>(sq), occ)) &
        ~bocc;
    sc -= popcnt(a) * 1;
  }

  return sc;
}

static int king_safety(const std::array<Bitboard, 6>& wbbs, const std::array<Bitboard, 6>& bbbs,
                       Bitboard occ) {
  int sc = 0;
  Bitboard wk = wbbs[static_cast<int>(PieceType::King)];
  Bitboard bk = bbbs[static_cast<int>(PieceType::King)];
  int wksq = lsb_i(wk);
  int bksq = lsb_i(bk);

  if (wksq >= 0) {
    Bitboard shield = 0;
    int f = sq_file(wksq);
    int r = sq_rank(wksq);
    for (int dr = 1; dr <= 2; ++dr) {
      int nr = r + dr;
      if (nr >= 8) continue;
      for (int df = -1; df <= 1; ++df) {
        int nf = f + df;
        if (nf < 0 || nf > 7) continue;
        shield |= sq_bb(static_cast<core::Square>((nr << 3) | nf));
      }
    }
    sc += popcnt(wbbs[static_cast<int>(PieceType::Pawn)] & shield) * 10;
  }

  if (bksq >= 0) {
    Bitboard shield = 0;
    int f = sq_file(bksq);
    int r = sq_rank(bksq);
    for (int dr = 1; dr <= 2; ++dr) {
      int nr = r - dr;
      if (nr < 0) continue;
      for (int df = -1; df <= 1; ++df) {
        int nf = f + df;
        if (nf < 0 || nf > 7) continue;
        shield |= sq_bb(static_cast<core::Square>((nr << 3) | nf));
      }
    }
    sc -= popcnt(bbbs[static_cast<int>(PieceType::Pawn)] & shield) * 10;
  }

  auto tropism = [&](int king_sq, const std::array<Bitboard, 6>& enemy_bb) -> int {
    if (king_sq < 0) return 0;
    int sum = 0;
    int kf = sq_file(king_sq), kr = sq_rank(king_sq);
    Bitboard area = 0;
    for (int dr = -2; dr <= 2; ++dr)
      for (int df = -2; df <= 2; ++df) {
        int nr = kr + dr;
        int nf = kf + df;
        if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8)
          area |= sq_bb(static_cast<core::Square>((nr << 3) | nf));
      }
    Bitboard kn = enemy_bb[static_cast<int>(PieceType::Knight)];
    sum += popcnt(kn & area) * 25;
    Bitboard heavy = enemy_bb[static_cast<int>(PieceType::Queen)] |
                     enemy_bb[static_cast<int>(PieceType::Rook)] |
                     enemy_bb[static_cast<int>(PieceType::Bishop)];
    sum += popcnt(heavy & area) * 12;
    return sum;
  };
  sc -= tropism(wksq, bbbs);
  sc += tropism(bksq, wbbs);

  auto shield_files_penalty = [&](int ksq, const std::array<Bitboard, 6>& own,
                                  bool whiteKing) -> int {
    if (ksq < 0) return 0;
    int f = sq_file(ksq), r = sq_rank(ksq);
    int files[3] = {std::max(0, f - 1), f, std::min(7, f + 1)};
    int miss = 0;
    for (int i = 0; i < 3; i++) {
      int ff = files[i];
      Bitboard mask = 0;
      if (whiteKing) {
        for (int rr = r + 1; rr < 8; ++rr) mask |= sq_bb(static_cast<core::Square>((rr << 3) | ff));
        if ((own[(int)PieceType::Pawn] & mask) == 0) ++miss;
      } else {
        for (int rr = r - 1; rr >= 0; --rr)
          mask |= sq_bb(static_cast<core::Square>((rr << 3) | ff));
        if ((own[(int)PieceType::Pawn] & mask) == 0) ++miss;
      }
    }
    return miss * 8;  // 0..24
  };

  if (wksq >= 0) sc -= shield_files_penalty(wksq, wbbs, true);
  if (bksq >= 0) sc += shield_files_penalty(bksq, bbbs, false);

  return sc;
}

static int bishop_pair(const std::array<Bitboard, 6>& wbbs, const std::array<Bitboard, 6>& bbbs) {
  int score = 0;
  if (popcnt(wbbs[static_cast<int>(PieceType::Bishop)]) >= 2) score += BISHOP_PAIR_BONUS;
  if (popcnt(bbbs[static_cast<int>(PieceType::Bishop)]) >= 2) score -= BISHOP_PAIR_BONUS;
  return score;
}

static int development_score(const std::array<Bitboard, 6>& wbbs,
                             const std::array<Bitboard, 6>& bbbs) {
  Bitboard white_minors =
      wbbs[static_cast<int>(PieceType::Knight)] | wbbs[static_cast<int>(PieceType::Bishop)];
  Bitboard black_minors =
      bbbs[static_cast<int>(PieceType::Knight)] | bbbs[static_cast<int>(PieceType::Bishop)];

  Bitboard white_initial =
      sq_bb(static_cast<core::Square>(1)) | sq_bb(static_cast<core::Square>(6)) |
      sq_bb(static_cast<core::Square>(2)) | sq_bb(static_cast<core::Square>(5));
  Bitboard black_initial =
      sq_bb(static_cast<core::Square>(57)) | sq_bb(static_cast<core::Square>(62)) |
      sq_bb(static_cast<core::Square>(58)) | sq_bb(static_cast<core::Square>(61));

  int white_undeveloped = popcnt(white_minors & white_initial);
  int black_undeveloped = popcnt(black_minors & black_initial);

  return (black_undeveloped - white_undeveloped) * DEVELOPMENT_PENALTY;
}

static int knight_rim(const std::array<Bitboard, 6>& wbbs, const std::array<Bitboard, 6>& bbbs) {
  int score = 0;
  Bitboard a_mask = masks.file_mask[0];
  Bitboard h_mask = masks.file_mask[7];

  Bitboard wn = wbbs[static_cast<int>(PieceType::Knight)];
  Bitboard bn = bbbs[static_cast<int>(PieceType::Knight)];

  score -= popcnt(wn & (a_mask | h_mask)) * KNIGHT_RIM_PENALTY;
  score += popcnt(bn & (a_mask | h_mask)) * KNIGHT_RIM_PENALTY;

  return score;
}

static int center_and_outposts(const std::array<Bitboard, 6>& wbbs,
                               const std::array<Bitboard, 6>& bbbs, Bitboard occ) {
  int score = 0;

  score += popcnt(wbbs[static_cast<int>(PieceType::Knight)] & CENTER_MASK) * OUTPOST_KNIGHT_BONUS;
  score -= popcnt(bbbs[static_cast<int>(PieceType::Knight)] & CENTER_MASK) * OUTPOST_KNIGHT_BONUS;

  Bitboard wn = wbbs[static_cast<int>(PieceType::Knight)];
  while (wn) {
    int sq = lsb_i(wn);
    wn &= wn - 1;
    if (knight_attacks_from(static_cast<core::Square>(sq)) & CENTER_MASK)
      score += CENTER_CONTROL_BONUS;
  }
  Bitboard bn = bbbs[static_cast<int>(PieceType::Knight)];
  while (bn) {
    int sq = lsb_i(bn);
    bn &= bn - 1;
    if (knight_attacks_from(static_cast<core::Square>(sq)) & CENTER_MASK)
      score -= CENTER_CONTROL_BONUS;
  }

  Bitboard wb = wbbs[static_cast<int>(PieceType::Bishop)];
  while (wb) {
    int sq = lsb_i(wb);
    wb &= wb - 1;
    Bitboard a = magic::sliding_attacks(magic::Slider::Bishop, static_cast<core::Square>(sq), occ);
    if (a & CENTER_MASK) score += CENTER_CONTROL_BONUS;
  }
  Bitboard bb = bbbs[static_cast<int>(PieceType::Bishop)];
  while (bb) {
    int sq = lsb_i(bb);
    bb &= bb - 1;
    Bitboard a = magic::sliding_attacks(magic::Slider::Bishop, static_cast<core::Square>(sq), occ);
    if (a & CENTER_MASK) score -= CENTER_CONTROL_BONUS;
  }

  Bitboard wq = wbbs[static_cast<int>(PieceType::Queen)];
  while (wq) {
    int sq = lsb_i(wq);
    wq &= wq - 1;
    Bitboard a =
        (magic::sliding_attacks(magic::Slider::Rook, static_cast<core::Square>(sq), occ) |
         magic::sliding_attacks(magic::Slider::Bishop, static_cast<core::Square>(sq), occ));
    if (a & CENTER_MASK) score += CENTER_CONTROL_BONUS;
  }
  Bitboard bq = bbbs[static_cast<int>(PieceType::Queen)];
  while (bq) {
    int sq = lsb_i(bq);
    bq &= bq - 1;
    Bitboard a =
        (magic::sliding_attacks(magic::Slider::Rook, static_cast<core::Square>(sq), occ) |
         magic::sliding_attacks(magic::Slider::Bishop, static_cast<core::Square>(sq), occ));
    if (a & CENTER_MASK) score -= CENTER_CONTROL_BONUS;
  }

  return score;
}

static int rook_activity(const std::array<Bitboard, 6>& wbbs, const std::array<Bitboard, 6>& bbbs,
                         Bitboard wp, Bitboard bp) {
  int score = 0;
  Bitboard wr = wbbs[static_cast<int>(PieceType::Rook)];
  Bitboard br = bbbs[static_cast<int>(PieceType::Rook)];

  auto file_of_sq = [&](int sq) -> Bitboard { return masks.file_mask[sq]; };
  auto rank_of_sq = [&](int sq) -> int { return sq_rank(sq); };

  Bitboard tmp_wr = wr;
  while (tmp_wr) {
    int sq = lsb_i(tmp_wr);
    tmp_wr &= tmp_wr - 1;
    Bitboard file = file_of_sq(sq);
    bool has_white_pawn = (file & wp) != 0;
    bool has_black_pawn = (file & bp) != 0;
    if (!has_white_pawn && !has_black_pawn)
      score += ROOK_OPEN_FILE_BONUS;
    else if (!has_white_pawn && has_black_pawn)
      score += ROOK_SEMI_OPEN_FILE_BONUS;
    if (rank_of_sq(sq) == 6) score += ROOK_ON_SEVENTH_BONUS;
  }

  Bitboard tmp_br = br;
  while (tmp_br) {
    int sq = lsb_i(tmp_br);
    tmp_br &= tmp_br - 1;
    Bitboard file = file_of_sq(sq);
    bool has_white_pawn = (file & wp) != 0;
    bool has_black_pawn = (file & bp) != 0;
    if (!has_white_pawn && !has_black_pawn)
      score -= ROOK_OPEN_FILE_BONUS;
    else if (!has_black_pawn && has_white_pawn)
      score -= ROOK_SEMI_OPEN_FILE_BONUS;
    if (rank_of_sq(sq) == 1) score -= ROOK_ON_SEVENTH_BONUS;
  }

  Bitboard wr_all = wr;
  if (popcnt(wr_all) == 2) {
    int sq1 = lsb_i(wr_all);
    Bitboard tmp = wr_all & (wr_all - 1);
    int sq2 = lsb_i(tmp);
    if (sq1 >= 0 && sq2 >= 0) {
      if (sq_file(sq1) == sq_file(sq2) || sq_rank(sq1) == sq_rank(sq2)) {
        Bitboard f1 = file_of_sq(sq1), f2 = file_of_sq(sq2);
        if (((f1 & wp) == 0) || ((f2 & wp) == 0)) score += CONNECTED_ROOKS_BONUS;
      }
    }
  }

  Bitboard br_all = br;
  if (popcnt(br_all) == 2) {
    int sq1 = lsb_i(br_all);
    Bitboard tmp = br_all & (br_all - 1);
    int sq2 = lsb_i(tmp);
    if (sq1 >= 0 && sq2 >= 0) {
      if (sq_file(sq1) == sq_file(sq2) || sq_rank(sq1) == sq_rank(sq2)) {
        Bitboard f1 = file_of_sq(sq1), f2 = file_of_sq(sq2);
        if (((f1 & bp) == 0) || ((f2 & bp) == 0)) score -= CONNECTED_ROOKS_BONUS;
      }
    }
  }

  return score;
}

constexpr size_t EVAL_CACHE_BITS = 14;
constexpr size_t EVAL_CACHE_SIZE = 1ULL << EVAL_CACHE_BITS;
// === NEU: je Slot ein Seqlock (even = stabil, odd = Writer aktiv)
struct EvalEntry {
  std::atomic<uint64_t> key{0};
  std::atomic<int32_t> score{0};
  std::atomic<uint32_t> age{0};
  std::atomic<uint32_t> seq{0};  // <--- NEU
};

struct PawnEntry {
  std::atomic<uint64_t> key{0};
  std::atomic<int32_t> pawn_score{0};
  std::atomic<uint32_t> age{0};
  std::atomic<uint32_t> seq{0};  // <--- NEU
};

constexpr size_t PAWN_CACHE_BITS = 12;
constexpr size_t PAWN_CACHE_SIZE = 1ULL << PAWN_CACHE_BITS;

struct Evaluator::Impl {
  std::array<EvalEntry, EVAL_CACHE_SIZE> eval_cache;
  std::array<PawnEntry, PAWN_CACHE_SIZE> pawn_cache;
  std::atomic<uint32_t> global_age{1};
  Impl() {}
  inline void incr_age() {
    uint32_t g = global_age.fetch_add(1, std::memory_order_relaxed) + 1;
    if (g == 0) global_age.store(1, std::memory_order_relaxed);
  }
};

Evaluator::Evaluator() noexcept {
  m_impl = new Impl();
}
Evaluator::~Evaluator() noexcept {
  delete m_impl;
}

void Evaluator::clearCaches() const noexcept {
  if (!m_impl) return;
  for (auto& e : m_impl->eval_cache) {
    e.key.store(0, std::memory_order_relaxed);
    e.score.store(0, std::memory_order_relaxed);
    e.age.store(0, std::memory_order_relaxed);
    e.seq.store(0, std::memory_order_relaxed);  // <--- NEU
  }
  for (auto& p : m_impl->pawn_cache) {
    p.key.store(0, std::memory_order_relaxed);
    p.pawn_score.store(0, std::memory_order_relaxed);
    p.age.store(0, std::memory_order_relaxed);
    p.seq.store(0, std::memory_order_relaxed);  // <--- NEU
  }
  m_impl->global_age.store(1, std::memory_order_relaxed);
}

static inline size_t eval_index_from_key(Bitboard key) noexcept {
  return static_cast<size_t>(key) & (EVAL_CACHE_SIZE - 1);
}
static inline size_t pawn_index_from_key(Bitboard key) noexcept {
  return static_cast<size_t>(key) & (PAWN_CACHE_SIZE - 1);
}

int Evaluator::evaluate(model::Position& pos) const {
  init_masks_if_needed();

  const Board& b = pos.getBoard();
  const uint64_t board_key = static_cast<uint64_t>(pos.hash());
  const uint64_t pawn_key = static_cast<uint64_t>(pos.getState().pawnKey);

  // --- Eval-Cache: Fast-Path (Seqlock-gesichert) ---
  {
    const size_t ei = eval_index_from_key(board_key);
    const auto& slot = m_impl->eval_cache[ei];

    // ein paar Versuche reichen völlig
    for (int tries = 0; tries < 3; ++tries) {
      const uint32_t s1 = slot.seq.load(std::memory_order_acquire);
      if (s1 & 1u) continue;  // Writer aktiv

      const uint64_t k = slot.key.load(std::memory_order_acquire);
      const int32_t sc = slot.score.load(std::memory_order_acquire);

      const uint32_t s2 = slot.seq.load(std::memory_order_acquire);

      if (s1 == s2 && !(s2 & 1u) && k == board_key) {
        return sc;  // konsistent
      }
      if (k != board_key) break;  // anderer Key => Miss
    }
  }

  // --- Pawn-Cache (Seqlock) ---
  int pawn_score = std::numeric_limits<int>::min();
  {
    const size_t pi = pawn_index_from_key(pawn_key);
    const auto& ps = m_impl->pawn_cache[pi];

    for (int tries = 0; tries < 3; ++tries) {
      const uint32_t s1 = ps.seq.load(std::memory_order_acquire);
      if (s1 & 1u) continue;

      const uint64_t k = ps.key.load(std::memory_order_acquire);
      const int32_t sc = ps.pawn_score.load(std::memory_order_acquire);

      const uint32_t s2 = ps.seq.load(std::memory_order_acquire);

      if (s1 == s2 && !(s2 & 1u) && k == pawn_key) {
        pawn_score = sc;
        break;
      }
      if (k != pawn_key) break;
    }
  }

  // --- Bitboards einsammeln ---
  std::array<Bitboard, 6> wbbs{}, bbbs{};
  for (int pt = 0; pt < 6; ++pt) {
    wbbs[pt] = b.getPieces(Color::White, static_cast<PieceType>(pt));
    bbbs[pt] = b.getPieces(Color::Black, static_cast<PieceType>(pt));
  }
  const Bitboard occ = b.getAllPieces();
  const Bitboard wocc = b.getPieces(Color::White);
  const Bitboard bocc = b.getPieces(Color::Black);

  // --- Material + PST + Phase ---
  int mg = 0, eg = 0, phase = 0;
  material_pst_phase(wbbs, bbbs, mg, eg, phase);

  if (pawn_score == std::numeric_limits<int>::min()) {
    pawn_score = pawn_structure(wbbs[static_cast<int>(PieceType::Pawn)],
                                bbbs[static_cast<int>(PieceType::Pawn)]);
  }

  // --- weitere Terme ---
  const int mob = std::clamp(mobility(occ, wocc, bocc, wbbs, bbbs), -200, 200);
  const int ks = king_safety(wbbs, bbbs, occ);
  const int bp = bishop_pair(wbbs, bbbs);
  const int dev = development_score(wbbs, bbbs);
  const int rim = knight_rim(wbbs, bbbs);
  const int cent = center_and_outposts(wbbs, bbbs, occ);
  const int ract = rook_activity(wbbs, bbbs, wbbs[static_cast<int>(PieceType::Pawn)],
                                 bbbs[static_cast<int>(PieceType::Pawn)]);

  // MG/EG additive Terme
  const int mg_add = pawn_score + mob + ks + bp + dev + rim + cent + ract;
  const int eg_add = (pawn_score / 2) + (mob / 3) + /* (ks / 4) */ 0 + (bp / 2) + (dev / 4) +
                     (cent / 2) + (ract / 3);

  mg += mg_add;
  eg += eg_add;

  // --- Phasenmischung ---
  constexpr int MAX_PHASE = 24;
  const int cur_phase = std::clamp(phase, 0, MAX_PHASE);
  const int mg_w = (cur_phase * 256) / MAX_PHASE;
  const int eg_w = 256 - mg_w;

  int final_score = ((mg * mg_w) + (eg * eg_w)) >> 8;

  // --- Tempo (board_key enthält side-to-move via Zobrist::side)
  const int tempo = (pos.getState().sideToMove == Color::White ? +TEMPO_BONUS : -TEMPO_BONUS);
  final_score += tempo;

  // --- globales Alter (optional, rein heuristisch) ---
  uint32_t age = m_impl->global_age.fetch_add(1, std::memory_order_relaxed) + 1;
  if (age == 0) {
    m_impl->global_age.store(1, std::memory_order_relaxed);
    age = 1;
  }

  // --- Pawn-Cache Store (Seqlock) ---
  {
    const size_t pi = pawn_index_from_key(pawn_key);
    auto& pe = m_impl->pawn_cache[pi];

    const uint32_t s0 = pe.seq.load(std::memory_order_relaxed);
    pe.seq.store(s0 | 1u, std::memory_order_release);  // begin (odd)

    pe.pawn_score.store(pawn_score, std::memory_order_relaxed);
    pe.age.store(age, std::memory_order_relaxed);
    pe.key.store(pawn_key, std::memory_order_release);  // publish key

    pe.seq.store((s0 | 1u) + 1u, std::memory_order_release);  // end (even)
  }

  // --- Eval-Cache Store (Seqlock) ---
  {
    const size_t ei = eval_index_from_key(board_key);
    auto& ee = m_impl->eval_cache[ei];

    const uint32_t s0 = ee.seq.load(std::memory_order_relaxed);
    ee.seq.store(s0 | 1u, std::memory_order_release);  // begin (odd)

    ee.score.store(final_score, std::memory_order_relaxed);
    ee.age.store(age, std::memory_order_relaxed);
    ee.key.store(board_key, std::memory_order_release);  // publish key

    ee.seq.store((s0 | 1u) + 1u, std::memory_order_release);  // end (even)
  }

  return final_score;
}

}  // namespace lilia::engine
