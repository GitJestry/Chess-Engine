#include "lilia/engine/eval.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <cstdint>
#include <limits>
#include <mutex>

#include "lilia/engine/config.hpp"
#include "lilia/model/core/bitboard.hpp"
#include "lilia/model/core/magic.hpp"
#include "lilia/model/position.hpp"

using namespace lilia::core;
using namespace lilia::model;
using namespace lilia::model::bb;

namespace lilia::engine {

// =============================================================================
// Utility
// =============================================================================
inline constexpr int mirror_sq_black(int sq) noexcept {
  return sq ^ 56;
}
inline int popcnt(Bitboard b) noexcept {
  return popcount(b);
}
inline int lsb_i(Bitboard b) noexcept {
  return b ? ctz64(b) : -1;
}
inline int msb_i(Bitboard b) noexcept {
  return b ? 63 - clz64(b) : -1;
}
inline int clampi(int x, int lo, int hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}
inline int king_manhattan(int a, int b) {
  if (a < 0 || b < 0) return 7;
  return (std::abs((a & 7) - (b & 7)) + std::abs((a >> 3) - (b >> 3)));
}

// =============================================================================
// Values & phase
// =============================================================================
constexpr std::array<int, 6> VAL_MG = {82, 337, 365, 477, 1025, 0};
constexpr std::array<int, 6> VAL_EG = {94, 281, 297, 512, 936, 0};
constexpr std::array<int, 6> PHASE_W = {0, 1, 1, 2, 4, 0};
constexpr int MAX_PHASE = 24;  // sum both sides

constexpr int TEMPO_MG = 14;
constexpr int TEMPO_EG = 6;

// =============================================================================
// PSTs (deine Tabellen)
// =============================================================================
static constexpr std::array<int, 64> PST_P_MG = {
    0,  0,  0,  0,  0,  0,  0,  0,  6,  6,  2,  -6, -6, 2,  6,  6,  4,  -2, -3, 2,  2,  -3,
    -2, 4,  6,  8,  12, 16, 16, 12, 8,  6,  8,  12, 18, 24, 24, 18, 12, 8,  12, 18, 24, 28,
    28, 24, 18, 12, 12, 12, 12, 12, 12, 12, 12, 12, 0,  0,  0,  0,  0,  0,  0,  0};
static constexpr std::array<int, 64> PST_P_EG = {
    0,  0,  0,  0,  0,  0,  0,  0,  6,  8,  4,  -2, -2, 4,  8,  6,  6,  2,  2,  6,  6,  2,
    2,  6,  8,  12, 16, 20, 20, 16, 12, 8,  12, 18, 24, 30, 30, 24, 18, 12, 16, 24, 32, 40,
    40, 32, 24, 16, 10, 14, 18, 22, 22, 18, 14, 10, 0,  0,  0,  0,  0,  0,  0,  0};
static constexpr std::array<int, 64> PST_N_MG = {
    -50, -38, -28, -22, -22, -28, -38, -50, -32, -16, -4,  2,   2,   -4,  -16, -32,
    -24, -2,  12,  18,  18,  12,  -2,  -24, -20, 4,   18,  26,  26,  18,  4,   -20,
    -20, 4,   18,  26,  26,  18,  4,   -20, -24, -2,  12,  18,  18,  12,  -2,  -24,
    -34, -16, -4,  0,   0,   -4,  -16, -34, -46, -36, -28, -24, -24, -28, -36, -46};
static constexpr std::array<int, 64> PST_N_EG = {
    -36, -26, -18, -14, -14, -18, -26, -36, -26, -12, -2,  6,   6,   -2,  -12, -26,
    -18, -2,  10,  16,  16,  10,  -2,  -18, -14, 6,   16,  22,  22,  16,  6,   -14,
    -14, 6,   16,  22,  22,  16,  6,   -14, -18, -2,  10,  16,  16,  10,  -2,  -18,
    -26, -12, -2,  6,   6,   -2,  -12, -26, -36, -26, -18, -14, -14, -18, -26, -36};
static constexpr std::array<int, 64> PST_B_MG = {
    -26, -14, -10, -8, -8, -10, -14, -26, -12, -4,  2,  6,  6,  2,  -4,  -12,
    -8,  4,   10,  14, 14, 10,  4,   -8,  -6,  8,   14, 20, 20, 14, 8,   -6,
    -6,  8,   14,  20, 20, 14,  8,   -6,  -8,  4,   10, 14, 14, 10, 4,   -8,
    -12, -4,  2,   6,  6,  2,   -4,  -12, -24, -12, -8, -6, -6, -8, -12, -24};
static constexpr std::array<int, 64> PST_B_EG = {
    -18, -8, -4, -2, -2, -4, -8, -18, -8, 0,  8,  12, 12,  8,  0,  -8, -4, 8,  14, 20, 20, 14,
    8,   -4, -2, 12, 20, 26, 26, 20,  12, -2, -2, 12, 20,  26, 26, 20, 12, -2, -4, 8,  14, 20,
    20,  14, 8,  -4, -8, 0,  8,  12,  12, 8,  0,  -8, -16, -8, -4, -2, -2, -4, -8, -16};
static constexpr std::array<int, 64> PST_R_MG = {
    0,  2,  3,  4,  4, 3, 2, 0, -2, 0,  2,  4,  4, 2, 0, -2, -3, -1, 0,  2,  2, 0,
    -1, -3, -4, -1, 1, 2, 2, 1, -1, -4, -4, -1, 1, 2, 2, 1,  -1, -4, -3, -1, 0, 2,
    2,  0,  -1, -3, 4, 6, 6, 8, 8,  6,  6,  4,  2, 4, 4, 6,  6,  4,  4,  2};
static constexpr std::array<int, 64> PST_R_EG = {
    2, 4,  6,  8,  8, 6, 4, 2, 0, 2,  4,  6, 6, 4, 2, 0,  -1, 1,  2,  4, 4, 2,
    1, -1, -1, 1,  2, 4, 4, 2, 1, -1, -1, 1, 2, 4, 4, 2,  1,  -1, -1, 1, 2, 4,
    4, 2,  1,  -1, 3, 5, 7, 9, 9, 7,  5,  3, 4, 6, 8, 10, 10, 8,  6,  4};
static constexpr std::array<int, 64> PST_Q_MG = {
    -24, -16, -12, -8, -8, -12, -16, -24, -16, -8,  -4,  -2, -2, -4,  -8,  -16,
    -12, -4,  2,   4,  4,  2,   -4,  -12, -8,  -2,  4,   6,  6,  4,   -2,  -8,
    -8,  -2,  4,   6,  6,  4,   -2,  -8,  -12, -4,  2,   4,  4,  2,   -4,  -12,
    -16, -8,  -4,  -2, -2, -4,  -8,  -16, -24, -16, -12, -8, -8, -12, -16, -24};
static constexpr std::array<int, 64> PST_Q_EG = {
    -10, -6, -2, 0,  0,  -2, -6, -10, -6, -2, 2,  4,  4,   2,  -2, -6, -2, 2,  6,  8,  8, 6,
    2,   -2, 0,  4,  8,  12, 12, 8,   4,  0,  0,  4,  8,   12, 12, 8,  4,  0,  -2, 2,  6, 8,
    8,   6,  2,  -2, -6, -2, 2,  4,   4,  2,  -2, -6, -10, -6, -2, 0,  0,  -2, -6, -10};
static constexpr std::array<int, 64> PST_K_MG = {
    -40, -48, -52, -56, -56, -52, -48, -40, -32, -40, -44, -50, -50, -44, -40, -32,
    -24, -32, -36, -44, -44, -36, -32, -24, -12, -20, -28, -36, -36, -28, -20, -12,
    0,   -8,  -18, -28, -28, -18, -8,  0,   10,  18,  4,   -10, -10, 4,   18,  10,
    20,  28,  18,  6,   6,   18,  28,  20,  28,  38,  28,  12,  12,  28,  38,  28};
static constexpr std::array<int, 64> PST_K_EG = {
    -8, -4, -4, -2, -2, -4, -4, -8, -4, 2,  4,  6,  6,  4,  2,  -4, -4, 4,  10, 12, 12, 10,
    4,  -4, -2, 6,  12, 18, 18, 12, 6,  -2, -2, 6,  12, 18, 18, 12, 6,  -2, -4, 4,  10, 12,
    12, 10, 4,  -4, -4, 2,  4,  6,  6,  4,  2,  -4, -8, -4, -4, -2, -2, -4, -4, -8};

static inline int pst_mg(PieceType pt, int sq) {
  switch (pt) {
    case PieceType::Pawn:
      return PST_P_MG[sq];
    case PieceType::Knight:
      return PST_N_MG[sq];
    case PieceType::Bishop:
      return PST_B_MG[sq];
    case PieceType::Rook:
      return PST_R_MG[sq];
    case PieceType::Queen:
      return PST_Q_MG[sq];
    case PieceType::King:
      return PST_K_MG[sq];
    default:
      return 0;
  }
}
static inline int pst_eg(PieceType pt, int sq) {
  switch (pt) {
    case PieceType::Pawn:
      return PST_P_EG[sq];
    case PieceType::Knight:
      return PST_N_EG[sq];
    case PieceType::Bishop:
      return PST_B_EG[sq];
    case PieceType::Rook:
      return PST_R_EG[sq];
    case PieceType::Queen:
      return PST_Q_EG[sq];
    case PieceType::King:
      return PST_K_EG[sq];
    default:
      return 0;
  }
}

// =============================================================================
// Masks
// =============================================================================
struct Masks {
  std::array<Bitboard, 64> file{};
  std::array<Bitboard, 64> adjFiles{};
  std::array<Bitboard, 64> wPassed{}, bPassed{}, wFront{}, bFront{};
  std::array<Bitboard, 64> kingRing{};
  std::array<Bitboard, 64> wShield{}, bShield{};
  std::array<Bitboard, 64> frontSpanW{}, frontSpanB{};
};
static Masks M;
static std::once_flag once_masks;
static void init_masks() {
  std::call_once(once_masks, [] {
    for (int sq = 0; sq < 64; ++sq) {
      int f = file_of(sq), r = rank_of(sq);
      Bitboard fm = 0;
      for (int rr = 0; rr < 8; ++rr) fm |= sq_bb((Square)((rr << 3) | f));
      M.file[sq] = fm;
      Bitboard adj = 0;
      if (f > 0)
        for (int rr = 0; rr < 8; ++rr) adj |= sq_bb((Square)((rr << 3) | (f - 1)));
      if (f < 7)
        for (int rr = 0; rr < 8; ++rr) adj |= sq_bb((Square)((rr << 3) | (f + 1)));
      M.adjFiles[sq] = adj;

      Bitboard pw = 0;
      for (int rr = r + 1; rr < 8; ++rr)
        for (int ff = std::max(0, f - 1); ff <= std::min(7, f + 1); ++ff)
          pw |= sq_bb((Square)((rr << 3) | ff));
      Bitboard pb = 0;
      for (int rr = r - 1; rr >= 0; --rr)
        for (int ff = std::max(0, f - 1); ff <= std::min(7, f + 1); ++ff)
          pb |= sq_bb((Square)((rr << 3) | ff));
      M.wPassed[sq] = pw;
      M.bPassed[sq] = pb;

      Bitboard wf = 0;
      for (int rr = r + 1; rr < 8; ++rr) wf |= sq_bb((Square)((rr << 3) | f));
      M.wFront[sq] = wf;
      M.frontSpanW[sq] = wf;

      Bitboard bf = 0;
      for (int rr = r - 1; rr >= 0; --rr) bf |= sq_bb((Square)((rr << 3) | f));
      M.bFront[sq] = bf;
      M.frontSpanB[sq] = bf;

      Bitboard ring = 0;
      for (int dr = -2; dr <= 2; ++dr)
        for (int df = -2; df <= 2; ++df) {
          int nr = r + dr, nf = f + df;
          if (0 <= nr && nr < 8 && 0 <= nf && nf < 8) ring |= sq_bb((Square)((nr << 3) | nf));
        }
      M.kingRing[sq] = ring;

      auto mkShield = [&](bool w) {
        Bitboard sh = 0;
        if (w) {
          for (int dr = 1; dr <= 2; ++dr) {
            int nr = r + dr;
            if (nr >= 8) break;
            for (int df = -1; df <= 1; ++df) {
              int nf = f + df;
              if (0 <= nf && nf < 8) sh |= sq_bb((Square)((nr << 3) | nf));
            }
          }
        } else {
          for (int dr = 1; dr <= 2; ++dr) {
            int nr = r - dr;
            if (nr < 0) break;
            for (int df = -1; df <= 1; ++df) {
              int nf = f + df;
              if (0 <= nf && nf < 8) sh |= sq_bb((Square)((nr << 3) | nf));
            }
          }
        }
        return sh;
      };
      M.wShield[sq] = mkShield(true);
      M.bShield[sq] = mkShield(false);
    }
  });
}

// =============================================================================
// Mobility profiles
// =============================================================================
static constexpr int KN_MOB_MG[9] = {-16, -8, -4, 0, 4, 8, 12, 16, 18};
static constexpr int KN_MOB_EG[9] = {-12, -6, -2, 2, 6, 10, 12, 14, 16};
static constexpr int BI_MOB_MG[14] = {-22, -12, -6, -2, 2, 6, 10, 14, 18, 22, 24, 26, 28, 30};
static constexpr int BI_MOB_EG[14] = {-18, -10, -4, 0, 4, 8, 12, 16, 20, 24, 26, 28, 30, 32};
static constexpr int RO_MOB_MG[15] = {-20, -12, -6, -2, 2, 6, 10, 14, 18, 22, 26, 30, 32, 34, 36};
static constexpr int RO_MOB_EG[15] = {-10, -6, -2, 2, 6, 10, 14, 18, 22, 26, 30, 34, 36, 38, 40};
static constexpr int QU_MOB_MG[28] = {-10, -8, -6, -4, -2, 0,  2,  4,  6,  8,  10, 12, 14, 16,
                                      18,  20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44};
static constexpr int QU_MOB_EG[28] = {-6, -4, -2, 0,  2,  4,  6,  8,  10, 12, 14, 16, 18, 20,
                                      22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48};

// =============================================================================
// Tunables – structure & style
// =============================================================================
constexpr Bitboard CENTER4 =
    sq_bb(Square(27)) | sq_bb(Square(28)) | sq_bb(Square(35)) | sq_bb(Square(36));

// Pawns
constexpr int ISO_P = 12;
constexpr int DOUBLED_P = 16;
constexpr int BACKWARD_P = 10;
constexpr int PHALANX = 6;
constexpr int CANDIDATE_P = 6;
constexpr int CONNECTED_PASSERS = 12;
constexpr int PASSED_MG[8] = {0, 8, 16, 26, 44, 70, 110, 0};
constexpr int PASSED_EG[8] = {0, 12, 22, 36, 56, 85, 130, 0};
constexpr int PASS_BLOCK = 8;
constexpr int PASS_SUPP = 6;
constexpr int PASS_FREE = 8;
constexpr int PASS_KBOOST = 6;
constexpr int PASS_KBLOCK = 6;

// King safety
constexpr int KS_W_N = 18, KS_W_B = 18, KS_W_R = 10, KS_W_Q = 38;
constexpr int KS_RING_BONUS = 2, KS_MISS_SHIELD = 7, KS_OPEN_FILE = 12, KS_RQ_LOS = 6,
              KS_CLAMP = 220;

// King pawn shelter / storm
static constexpr int SHELTER[8] = {0, 0, 2, 6, 12, 18, 24, 28};
static constexpr int STORM[8] = {0, 6, 10, 14, 18, 22, 26, 30};

// Pieces/style
constexpr int BISHOP_PAIR = 38;
constexpr int BAD_BISHOP_PER_PAWN = 2;
constexpr int OUTPOST_KN = 24;
constexpr int CENTER_CTRL = 6;
constexpr int KNIGHT_RIM = 12;
constexpr int ROOK_OPEN = 16;
constexpr int ROOK_SEMI = 8;
constexpr int ROOK_ON_7TH = 20;
constexpr int CONNECTED_ROOKS = 18;
constexpr int ROOK_BEHIND_PASSER = 18;

// Threats
constexpr int THR_PAWN_MINOR = 12, THR_PAWN_ROOK = 18, THR_PAWN_QUEEN = 26;
constexpr int HANG_MINOR = 14, HANG_ROOK = 20, HANG_QUEEN = 28;
constexpr int MINOR_ON_QUEEN = 8;

// Space
constexpr int SPACE_BASE = 2;

// Endgame scaling
constexpr int OPP_BISHOPS_SCALE = 192;  // /256

// Material imbalance (leicht)
struct MaterialCounts {
  int P[2]{}, N[2]{}, B[2]{}, R[2]{}, Q[2]{};
};
static int material_imbalance(const MaterialCounts& mc) {
  auto s = [&](int w, int b, int kw, int kb) {
    return (kw * (w * (w - 1)) / 2) - (kb * (b * (b - 1)) / 2);
  };
  int sc = 0;
  sc += s(mc.N[0], mc.N[1], 3, 3);
  sc += s(mc.B[0], mc.B[1], 4, 4);
  sc += (mc.B[0] >= 2 ? +16 : 0) + (mc.B[1] >= 2 ? -16 : 0);
  sc += (mc.R[0] * mc.N[0] * 2) - (mc.R[1] * mc.N[1] * 2);
  sc += (mc.R[0] * mc.B[0] * 1) - (mc.R[1] * mc.B[1] * 1);
  sc += (mc.Q[0] * mc.R[0] * (-2)) - (mc.Q[1] * mc.R[1] * (-2));
  return sc;
}

// =============================================================================
// Space
// =============================================================================
static int space_term(const std::array<Bitboard, 6>& W, const std::array<Bitboard, 6>& B) {
  Bitboard wocc = W[0] | W[1] | W[2] | W[3] | W[4] | W[5];
  Bitboard bocc = B[0] | B[1] | B[2] | B[3] | B[4] | B[5];
  Bitboard empty = ~(wocc | bocc);
  Bitboard wp = W[(int)PieceType::Pawn], bp = B[(int)PieceType::Pawn];
  Bitboard bPA = black_pawn_attacks(bp), wPA = white_pawn_attacks(wp);
  Bitboard wArea = (RANK_4 | RANK_5 | RANK_6) & empty & ~bPA;
  Bitboard bArea = (RANK_3 | RANK_4 | RANK_5) & empty & ~wPA;
  int wSafe = popcnt(wArea), bSafe = popcnt(bArea);
  int wMin = popcnt(W[(int)PieceType::Knight] | W[(int)PieceType::Bishop]);
  int bMin = popcnt(B[(int)PieceType::Knight] | B[(int)PieceType::Bishop]);
  int wScale = 2 + std::min(wMin, 4), bScale = 2 + std::min(bMin, 4);
  return SPACE_BASE * (wSafe * wScale - bSafe * bScale);
}

// =============================================================================
// Pawn structure (härter: backward/candidate/passers)
// =============================================================================
static int pawn_structure(Bitboard wp, Bitboard bp, int wK, int bK, Bitboard occ) {
  init_masks();
  int sc = 0;

  // Isolani & doubled (file-wise)
  for (int f = 0; f < 8; ++f) {
    Bitboard F = M.file[f];
    Bitboard ADJ = (f > 0 ? M.file[f - 1] : 0) | (f < 7 ? M.file[f + 1] : 0);
    int wc = popcnt(wp & F), bc = popcnt(bp & F);
    if (wc) {
      if ((wp & ADJ) == 0) sc -= ISO_P * wc;
      if (wc > 1) sc -= DOUBLED_P * (wc - 1);
    }
    if (bc) {
      if ((bp & ADJ) == 0) sc += ISO_P * bc;
      if (bc > 1) sc += DOUBLED_P * (bc - 1);
    }
  }

  Bitboard wPA = white_pawn_attacks(wp), bPA = black_pawn_attacks(bp);

  auto do_white = [&](int sq) {
    int f = file_of(sq), r = rank_of(sq);
    int front = sq + 8;
    bool blocked = (front <= 63) && (occ & sq_bb((Square)front));
    bool frontCtrl = (front <= 63) && (bPA & sq_bb((Square)front));
    Bitboard ownAdjAhead = (M.wPassed[sq] & ~M.wFront[sq]) & wp;
    if (!blocked && frontCtrl && ownAdjAhead == 0) sc -= BACKWARD_P;  // [FIX]
    if (f > 0 && (wp & sq_bb((Square)(sq - 1)))) sc += PHALANX;
    if (f < 7 && (wp & sq_bb((Square)(sq + 1)))) sc += PHALANX;
    bool passed = (M.wPassed[sq] & bp) == 0;
    bool candidate = !passed && ((M.wPassed[sq] & bp & ~M.wFront[sq]) == 0);
    if (candidate) sc += CANDIDATE_P;

    if (passed) {
      int mgB = PASSED_MG[r], egB = PASSED_EG[r];
      int stop = sq + 8;
      if (stop <= 63 && (occ & sq_bb((Square)stop))) {
        mgB -= PASS_BLOCK;
        egB -= PASS_BLOCK;
      }
      if (wPA & sq_bb((Square)sq)) {
        mgB += PASS_SUPP;
        egB += PASS_SUPP;
      }
      if ((M.wFront[sq] & occ) == 0) {
        mgB += PASS_FREE;
        egB += PASS_FREE;
      }
      if (king_manhattan(wK, sq) <= 3) {
        mgB += PASS_KBOOST;
        egB += PASS_KBOOST;
      }
      if (bK >= 0 &&
          (M.wFront[sq] | (stop <= 63 ? sq_bb((Square)stop) : 0ULL)) & sq_bb((Square)bK)) {
        mgB -= PASS_KBLOCK;
        egB -= PASS_KBLOCK;
      }
      sc += (mgB + egB) / 2;
    }
  };

  auto do_black = [&](int sq) {
    int f = file_of(sq), r = rank_of(sq);
    int front = sq - 8;
    bool blocked = (front >= 0) && (occ & sq_bb((Square)front));
    bool frontCtrl = (front >= 0) && (wPA & sq_bb((Square)front));
    Bitboard ownAdjAhead = (M.bPassed[sq] & ~M.bFront[sq]) & bp;
    if (!blocked && frontCtrl && ownAdjAhead == 0) sc += BACKWARD_P;  // [FIX]
    if (f > 0 && (bp & sq_bb((Square)(sq - 1)))) sc -= PHALANX;
    if (f < 7 && (bp & sq_bb((Square)(sq + 1)))) sc -= PHALANX;
    bool passed = (M.bPassed[sq] & wp) == 0;
    bool candidate = !passed && ((M.bPassed[sq] & wp & ~M.bFront[sq]) == 0);
    if (candidate) sc -= CANDIDATE_P;

    if (passed) {
      int mgB = PASSED_MG[7 - r], egB = PASSED_EG[7 - r];
      int stop = sq - 8;
      if (stop >= 0 && (occ & sq_bb((Square)stop))) {
        mgB -= PASS_BLOCK;
        egB -= PASS_BLOCK;
      }
      if (bPA & sq_bb((Square)sq)) {
        mgB += PASS_SUPP;
        egB += PASS_SUPP;
      }
      if ((M.bFront[sq] & occ) == 0) {
        mgB += PASS_FREE;
        egB += PASS_FREE;
      }
      if (king_manhattan(bK, sq) <= 3) {
        mgB += PASS_KBOOST;
        egB += PASS_KBOOST;
      }
      if (wK >= 0 &&
          (M.bFront[sq] | (stop >= 0 ? sq_bb((Square)stop) : 0ULL)) & sq_bb((Square)wK)) {
        mgB -= PASS_KBLOCK;
        egB -= PASS_KBLOCK;
      }
      sc -= (mgB + egB) / 2;
    }
  };

  Bitboard t = wp;
  while (t) {
    int s = lsb_i(t);
    t &= t - 1;
    do_white(s);
  }
  t = bp;
  while (t) {
    int s = lsb_i(t);
    t &= t - 1;
    do_black(s);
  }

  // connected passers
  Bitboard wPass = 0, bPass = 0;
  {
    Bitboard t2 = wp;
    while (t2) {
      int s = lsb_i(t2);
      t2 &= t2 - 1;
      if ((M.wPassed[s] & bp) == 0) wPass |= sq_bb((Square)s);
    }
    t2 = bp;
    while (t2) {
      int s = lsb_i(t2);
      t2 &= t2 - 1;
      if ((M.bPassed[s] & wp) == 0) bPass |= sq_bb((Square)s);
    }
  }
  Bitboard wConn = ((wPass << 1) & wPass) | ((wPass >> 1) & wPass);
  if (wConn) sc += CONNECTED_PASSERS * popcnt(wConn);
  Bitboard bConn = ((bPass << 1) & bPass) | ((bPass >> 1) & bPass);
  if (bConn) sc -= CONNECTED_PASSERS * popcnt(bConn);
  return sc;
}

// =============================================================================
// Mobility & attacks (safe mobility)
// =============================================================================
struct AttInfo {
  Bitboard wAll = 0, bAll = 0;
  int mg = 0, eg = 0;
};

static AttInfo mobility(Bitboard occ, Bitboard wocc, Bitboard bocc,
                        const std::array<Bitboard, 6>& W, const std::array<Bitboard, 6>& B) {
  AttInfo ai{};
  Bitboard wPA = white_pawn_attacks(W[(int)PieceType::Pawn]);
  Bitboard bPA = black_pawn_attacks(B[(int)PieceType::Pawn]);
  auto safeW = [&](Bitboard a) { return a & ~wocc & ~bPA; };
  auto safeB = [&](Bitboard a) { return a & ~bocc & ~wPA; };

  // Knights
  {
    Bitboard bb = W[(int)PieceType::Knight];
    while (bb) {
      int s = lsb_i(bb);
      bb &= bb - 1;
      Bitboard a = knight_attacks_from((Square)s);
      ai.wAll |= a;
      int c = clampi(popcount(safeW(a)), 0, 8);
      ai.mg += KN_MOB_MG[c];
      ai.eg += KN_MOB_EG[c];
    }
  }
  {
    Bitboard bb = B[(int)PieceType::Knight];
    while (bb) {
      int s = lsb_i(bb);
      bb &= bb - 1;
      Bitboard a = knight_attacks_from((Square)s);
      ai.bAll |= a;
      int c = clampi(popcount(safeB(a)), 0, 8);
      ai.mg -= KN_MOB_MG[c];
      ai.eg -= KN_MOB_EG[c];
    }
  }

  // Bishops
  {
    Bitboard bb = W[(int)PieceType::Bishop];
    while (bb) {
      int s = lsb_i(bb);
      bb &= bb - 1;
      Bitboard a = magic::sliding_attacks(magic::Slider::Bishop, (Square)s, occ);
      ai.wAll |= a;
      int c = clampi(popcount(safeW(a)), 0, 13);
      ai.mg += BI_MOB_MG[c];
      ai.eg += BI_MOB_EG[c];
    }
  }
  {
    Bitboard bb = B[(int)PieceType::Bishop];
    while (bb) {
      int s = lsb_i(bb);
      bb &= bb - 1;
      Bitboard a = magic::sliding_attacks(magic::Slider::Bishop, (Square)s, occ);
      ai.bAll |= a;
      int c = clampi(popcount(safeB(a)), 0, 13);
      ai.mg -= BI_MOB_MG[c];
      ai.eg -= BI_MOB_EG[c];
    }
  }

  // Rooks
  {
    Bitboard bb = W[(int)PieceType::Rook];
    while (bb) {
      int s = lsb_i(bb);
      bb &= bb - 1;
      Bitboard a = magic::sliding_attacks(magic::Slider::Rook, (Square)s, occ);
      ai.wAll |= a;
      int c = clampi(popcount(safeW(a)), 0, 14);
      ai.mg += RO_MOB_MG[c];
      ai.eg += RO_MOB_EG[c];
    }
  }
  {
    Bitboard bb = B[(int)PieceType::Rook];
    while (bb) {
      int s = lsb_i(bb);
      bb &= bb - 1;
      Bitboard a = magic::sliding_attacks(magic::Slider::Rook, (Square)s, occ);
      ai.bAll |= a;
      int c = clampi(popcount(safeB(a)), 0, 14);
      ai.mg -= RO_MOB_MG[c];
      ai.eg -= RO_MOB_EG[c];
    }
  }

  // Queens
  {
    Bitboard bb = W[(int)PieceType::Queen];
    while (bb) {
      int s = lsb_i(bb);
      bb &= bb - 1;
      Bitboard a = magic::sliding_attacks(magic::Slider::Rook, (Square)s, occ) |
                   magic::sliding_attacks(magic::Slider::Bishop, (Square)s, occ);
      ai.wAll |= a;
      int c = clampi(popcount(safeW(a)), 0, 27);
      ai.mg += QU_MOB_MG[c];
      ai.eg += QU_MOB_EG[c];
    }
  }
  {
    Bitboard bb = B[(int)PieceType::Queen];
    while (bb) {
      int s = lsb_i(bb);
      bb &= bb - 1;
      Bitboard a = magic::sliding_attacks(magic::Slider::Rook, (Square)s, occ) |
                   magic::sliding_attacks(magic::Slider::Bishop, (Square)s, occ);
      ai.bAll |= a;
      int c = clampi(popcount(safeB(a)), 0, 27);
      ai.mg -= QU_MOB_MG[c];
      ai.eg -= QU_MOB_EG[c];
    }
  }

  ai.mg = clampi(ai.mg, -900, 900);
  ai.eg = clampi(ai.eg, -900, 900);
  return ai;
}

// =============================================================================
// Threats & hanging  [REWORKED/FASTER/SAFER]
// =============================================================================
static int threats(const std::array<Bitboard, 6>& W, const std::array<Bitboard, 6>& B,
                   Bitboard wAll, Bitboard bAll, Bitboard occ) {
  int sc = 0;

  // Pawn threats
  Bitboard wPA = white_pawn_attacks(W[(int)PieceType::Pawn]);
  Bitboard bPA = black_pawn_attacks(B[(int)PieceType::Pawn]);

  auto pawn_threat_score = [&](Bitboard pa, const std::array<Bitboard, 6>& side) {
    int s = 0;
    if (pa & side[(int)PieceType::Knight]) s += THR_PAWN_MINOR;
    if (pa & side[(int)PieceType::Bishop]) s += THR_PAWN_MINOR;
    if (pa & side[(int)PieceType::Rook]) s += THR_PAWN_ROOK;
    if (pa & side[(int)PieceType::Queen]) s += THR_PAWN_QUEEN;
    return s;
  };

  sc += pawn_threat_score(wPA, B);
  sc -= pawn_threat_score(bPA, W);

  // Hanging: attacked by enemy, not defended by own (own pieces + pawn + king)
  int wKsq = lsb_i(W[5]), bKsq = lsb_i(B[5]);
  Bitboard wKAtt = (wKsq >= 0 ? king_attacks_from((Square)wKsq) : 0);
  Bitboard bKAtt = (bKsq >= 0 ? king_attacks_from((Square)bKsq) : 0);

  Bitboard defW = wAll | wPA | wKAtt;
  Bitboard defB = bAll | bPA | bKAtt;

  Bitboard wocc = W[0] | W[1] | W[2] | W[3] | W[4] | W[5];
  Bitboard bocc = B[0] | B[1] | B[2] | B[3] | B[4] | B[5];

  Bitboard wHang = (bAll & wocc) & ~defW;
  Bitboard bHang = (wAll & bocc) & ~defB;

  auto hang_score = [&](Bitboard h, const std::array<Bitboard, 6>& side) {
    int s = 0;
    if (h & side[1]) s += HANG_MINOR;
    if (h & side[2]) s += HANG_MINOR;
    if (h & side[3]) s += HANG_ROOK;
    if (h & side[4]) s += HANG_QUEEN;
    return s;
  };

  sc += hang_score(bHang, B);
  sc -= hang_score(wHang, W);

  // echte Minor-Angriffe auf die Dame (mit occ-Rays)
  Bitboard wMinorA = 0, bMinorA = 0;
  {
    Bitboard n = W[1];
    while (n) {
      int s = lsb_i(n);
      n &= n - 1;
      wMinorA |= knight_attacks_from((Square)s);
    }
    Bitboard bb = W[2];
    while (bb) {
      int s = lsb_i(bb);
      bb &= bb - 1;
      wMinorA |= magic::sliding_attacks(magic::Slider::Bishop, (Square)s, occ);
    }
  }
  {
    Bitboard n = B[1];
    while (n) {
      int s = lsb_i(n);
      n &= n - 1;
      bMinorA |= knight_attacks_from((Square)s);
    }
    Bitboard bb = B[2];
    while (bb) {
      int s = lsb_i(bb);
      bb &= bb - 1;
      bMinorA |= magic::sliding_attacks(magic::Slider::Bishop, (Square)s, occ);
    }
  }
  if (wMinorA & B[4]) sc += MINOR_ON_QUEEN;
  if (bMinorA & W[4]) sc -= MINOR_ON_QUEEN;

  return sc;
}

// =============================================================================
// King safety (ring-based; Feld-zählend; skaliert mit Queens)
// =============================================================================
static int king_safety_raw(const std::array<Bitboard, 6>& W, const std::array<Bitboard, 6>& B,
                           Bitboard occ) {
  init_masks();
  int wK = lsb_i(W[5]);
  int bK = lsb_i(B[5]);
  Bitboard wp = W[0], bp = B[0];
  Bitboard wPA = white_pawn_attacks(wp), bPA = black_pawn_attacks(bp);

  auto ring_attacks = [&](int ksq, const std::array<Bitboard, 6>& opp, bool kingIsWhite) {
    if (ksq < 0) return 0;
    Bitboard ring = M.kingRing[ksq];
    Bitboard ringSafe = ring & ~(kingIsWhite ? bPA : wPA);  // von gegnerischen Bauern befreit

    int power = 0;
    int cnt = 0;
    Bitboard cover = 0;

    // Knights
    {
      Bitboard bb = opp[1];
      while (bb) {
        int s = lsb_i(bb);
        bb &= bb - 1;
        Bitboard a = knight_attacks_from((Square)s) & ringSafe;
        int c = popcnt(a);
        if (c) {
          cnt += c;
          power += c * (KS_W_N - 2);
          cover |= a;
        }
      }
    }
    // Bishops
    {
      Bitboard bb = opp[2];
      while (bb) {
        int s = lsb_i(bb);
        bb &= bb - 1;
        Bitboard a = magic::sliding_attacks(magic::Slider::Bishop, (Square)s, occ) & ringSafe;
        int c = popcnt(a);
        if (c) {
          cnt += c;
          power += c * (KS_W_B - 2);
          cover |= a;
        }
      }
    }
    // Rooks
    {
      Bitboard bb = opp[3];
      while (bb) {
        int s = lsb_i(bb);
        bb &= bb - 1;
        Bitboard a = magic::sliding_attacks(magic::Slider::Rook, (Square)s, occ) & ringSafe;
        int c = popcnt(a);
        if (c) {
          cnt += c;
          power += c * KS_W_R;
          cover |= a;
        }
      }
    }
    // Queens
    {
      Bitboard bb = opp[4];
      while (bb) {
        int s = lsb_i(bb);
        bb &= bb - 1;
        Bitboard a = (magic::sliding_attacks(magic::Slider::Rook, (Square)s, occ) |
                      magic::sliding_attacks(magic::Slider::Bishop, (Square)s, occ)) &
                     ringSafe;
        int c = popcnt(a);
        if (c) {
          cnt += c;
          power += c * (KS_W_Q - 4);
          cover |= a;
        }
      }
    }

    int score = popcnt(cover) * KS_RING_BONUS + (power * std::min(cnt, 12)) / 12;

    // missing shield (eigene Bauern um den König)
    Bitboard shield = kingIsWhite ? M.wShield[ksq] : M.bShield[ksq];
    Bitboard ownP = kingIsWhite ? wp : bp;
    int missing = 6 - std::min(6, popcnt(ownP & shield));
    score += missing * KS_MISS_SHIELD;

    // (Half-)open file unter dem König
    Bitboard file = M.file[ksq];
    Bitboard oppP = kingIsWhite ? bp : wp;
    bool ownOn = (file & ownP), oppOn = (file & oppP);
    if (!ownOn && !oppOn)
      score += KS_OPEN_FILE;
    else if (!ownOn && oppOn)
      score += KS_OPEN_FILE / 2;

    // direkte R/Q Linie auf König
    Bitboard oppRQ = opp[3] | opp[4];
    Bitboard ray = magic::sliding_attacks(magic::Slider::Rook, (Square)ksq, occ);
    if (ray & oppRQ) score += KS_RQ_LOS;

    return std::min(score, KS_CLAMP);
  };

  int sc = 0;
  sc -= ring_attacks(wK, B, true);
  sc += ring_attacks(bK, W, false);
  return sc;
}

static int king_shelter_storm(const std::array<Bitboard, 6>& W, const std::array<Bitboard, 6>& B) {
  int wK = lsb_i(W[5]), bK = lsb_i(B[5]);
  if (wK < 0 || bK < 0) return 0;
  Bitboard wp = W[0], bp = B[0];
  auto fileShelter = [&](int ksq, bool white) {
    int f = file_of(ksq);
    int total = 0;
    for (int df = -1; df <= 1; ++df) {
      int ff = f + df;
      if (ff < 0 || ff > 7) continue;
      if (white) {
        Bitboard mask = 0;
        for (int r = rank_of(ksq) + 1; r < 8; ++r) mask |= sq_bb((Square)((r << 3) | ff));
        int nearR = (msb_i(mask & wp) >= 0 ? (msb_i(mask & wp) >> 3) : 8);
        int dist = clampi(nearR - rank_of(ksq), 0, 7);
        total += SHELTER[dist];
        Bitboard em = 0;
        for (int r = rank_of(ksq) - 1; r >= 0; --r) em |= sq_bb((Square)((r << 3) | ff));
        int nearER = (lsb_i(em & bp) >= 0 ? (lsb_i(em & bp) >> 3) : -1);
        int edist = clampi(rank_of(ksq) - nearER, 0, 7);
        total -= STORM[edist] / 2;
      } else {
        Bitboard mask = 0;
        for (int r = rank_of(ksq) - 1; r >= 0; --r) mask |= sq_bb((Square)((r << 3) | ff));
        int nearR = (lsb_i(mask & bp) >= 0 ? (lsb_i(mask & bp) >> 3) : -1);
        int dist = clampi(rank_of(ksq) - nearR, 0, 7);
        total += SHELTER[dist];
        Bitboard em = 0;
        for (int r = rank_of(ksq) + 1; r < 8; ++r) em |= sq_bb((Square)((r << 3) | ff));
        int nearER = (msb_i(em & wp) >= 0 ? (msb_i(em & wp) >> 3) : 8);
        int edist = clampi(nearER - rank_of(ksq), 0, 7);
        total -= STORM[edist] / 2;
      }
    }
    return total;
  };
  int sc = 0;
  sc += fileShelter(bK, false);
  sc -= fileShelter(wK, true);
  return sc / 2;
}

// =============================================================================
// Style terms
// =============================================================================
static int bishop_pair_term(const std::array<Bitboard, 6>& W, const std::array<Bitboard, 6>& B) {
  int s = 0;
  if (popcnt(W[2]) >= 2) s += BISHOP_PAIR;
  if (popcnt(B[2]) >= 2) s -= BISHOP_PAIR;
  return s;
}

static int bad_bishop(const std::array<Bitboard, 6>& W, const std::array<Bitboard, 6>& B) {
  auto is_light = [&](int sq) { return ((file_of(sq) + rank_of(sq)) & 1) != 0; };
  int sc = 0;
  auto apply = [&](const std::array<Bitboard, 6>& bb, int sign) {
    Bitboard paw = bb[0];
    int light = 0, dark = 0;
    Bitboard t = paw;
    while (t) {
      int s = lsb_i(t);
      t &= t - 1;
      (is_light(s) ? ++light : ++dark);
    }
    Bitboard bishops = bb[2];
    while (bishops) {
      int s = lsb_i(bishops);
      bishops &= bishops - 1;
      int same = is_light(s) ? light : dark;
      if (same > 4) sc += -(same - 4) * BAD_BISHOP_PER_PAWN * sign;
    }
  };
  apply(W, +1);
  apply(B, -1);
  return sc;
}

static int outposts_center(const std::array<Bitboard, 6>& W, const std::array<Bitboard, 6>& B) {
  int s = 0;
  Bitboard bPA = black_pawn_attacks(B[0]), wPA = white_pawn_attacks(W[0]);
  auto outW = [&](int sq) { return !(bPA & sq_bb((Square)sq)); };
  auto outB = [&](int sq) { return !(wPA & sq_bb((Square)sq)); };
  Bitboard t = W[1];
  while (t) {
    int sq = lsb_i(t);
    t &= t - 1;
    int add = 0;
    if (outW(sq)) add += OUTPOST_KN;
    if (knight_attacks_from((Square)sq) & CENTER4) add += CENTER_CTRL;
    if (sq_bb((Square)sq) & CENTER4) add += 6;
    s += add;
  }
  t = B[1];
  while (t) {
    int sq = lsb_i(t);
    t &= t - 1;
    int add = 0;
    if (outB(sq)) add += OUTPOST_KN;
    if (knight_attacks_from((Square)sq) & CENTER4) add += CENTER_CTRL;
    if (sq_bb((Square)sq) & CENTER4) add += 6;
    s -= add;
  }
  return s;
}

static int rim_knights(const std::array<Bitboard, 6>& W, const std::array<Bitboard, 6>& B) {
  init_masks();
  Bitboard aF = M.file[0], hF = M.file[7];
  int s = 0;
  s -= popcnt(W[1] & (aF | hF)) * KNIGHT_RIM;
  s += popcnt(B[1] & (aF | hF)) * KNIGHT_RIM;
  return s;
}

static int rook_activity(const std::array<Bitboard, 6>& W, const std::array<Bitboard, 6>& B,
                         Bitboard wp, Bitboard bp) {
  init_masks();
  int s = 0;
  Bitboard wr = W[3], br = B[3];
  auto rank = [&](int sq) { return rank_of(sq); };
  auto openScore = [&](int sq, bool white) {
    Bitboard f = M.file[sq];
    bool own = white ? (f & wp) : (f & bp);
    bool opp = white ? (f & bp) : (f & wp);
    if (!own && !opp) return ROOK_OPEN;
    if (!own && opp) return ROOK_SEMI;
    return 0;
  };
  Bitboard t = wr;
  while (t) {
    int sq = lsb_i(t);
    t &= t - 1;
    s += openScore(sq, true);
    if (rank(sq) == 6) {
      bool tgt = (B[5] & RANK_8) || (B[0] & RANK_7);
      if (tgt) s += ROOK_ON_7TH;
    }
  }
  t = br;
  while (t) {
    int sq = lsb_i(t);
    t &= t - 1;
    s -= openScore(sq, false);
    if (rank(sq) == 1) {
      bool tgt = (W[5] & RANK_1) || (W[0] & RANK_2);
      if (tgt) s -= ROOK_ON_7TH;
    }
  }
  auto connected = [&](Bitboard rooks, Bitboard occ) {
    if (popcnt(rooks) != 2) return false;
    int s1 = lsb_i(rooks);
    Bitboard r2 = rooks & (rooks - 1);
    int s2 = lsb_i(r2);
    Bitboard occ2 = occ & ~sq_bb((Square)s2);
    Bitboard ray = magic::sliding_attacks(magic::Slider::Rook, (Square)s1, occ2);
    return (ray & sq_bb((Square)s2)) != 0;
  };
  Bitboard occAll =
      W[0] | W[1] | W[2] | W[3] | W[4] | W[5] | B[0] | B[1] | B[2] | B[3] | B[4] | B[5];
  if (connected(wr, occAll)) s += CONNECTED_ROOKS;
  if (connected(br, occAll)) s -= CONNECTED_ROOKS;

  auto behind = [&](int rSq, int pSq, bool pawnWhite, int full, int half) {
    if (file_of(rSq) != file_of(pSq)) return 0;
    Bitboard ray = magic::sliding_attacks(magic::Slider::Rook, (Square)rSq, occAll);
    if (!(ray & sq_bb((Square)pSq))) return 0;
    if (pawnWhite)
      return (rank_of(rSq) < rank_of(pSq) ? full : half);
    else
      return (rank_of(rSq) > rank_of(pSq) ? full : half);
  };
  Bitboard wPass = 0, bPass = 0;
  {
    Bitboard t2 = wp;
    while (t2) {
      int ps = lsb_i(t2);
      t2 &= t2 - 1;
      if ((M.wPassed[ps] & bp) == 0) wPass |= sq_bb((Square)ps);
    }
    t2 = bp;
    while (t2) {
      int ps = lsb_i(t2);
      t2 &= t2 - 1;
      if ((M.bPassed[ps] & wp) == 0) bPass |= sq_bb((Square)ps);
    }
  }
  t = wr;
  while (t) {
    int rs = lsb_i(t);
    t &= t - 1;
    Bitboard f = M.file[rs] & wPass;
    while (f) {
      int ps = lsb_i(f);
      f &= f - 1;
      s += behind(rs, ps, true, ROOK_BEHIND_PASSER, ROOK_BEHIND_PASSER / 2);
    }
    f = M.file[rs] & bPass;
    while (f) {
      int ps = lsb_i(f);
      f &= f - 1;
      s += behind(rs, ps, false, ROOK_BEHIND_PASSER / 2, ROOK_BEHIND_PASSER / 3);
    }
  }
  t = br;
  while (t) {
    int rs = lsb_i(t);
    t &= t - 1;
    Bitboard f = M.file[rs] & bPass;
    while (f) {
      int ps = lsb_i(f);
      f &= f - 1;
      s -= behind(rs, ps, false, ROOK_BEHIND_PASSER, ROOK_BEHIND_PASSER / 2);
    }
    f = M.file[rs] & wPass;
    while (f) {
      int ps = lsb_i(f);
      f &= f - 1;
      s -= behind(rs, ps, true, ROOK_BEHIND_PASSER / 2, ROOK_BEHIND_PASSER / 3);
    }
  }
  return s;
}

// =============================================================================
// King tropism
// =============================================================================
static int king_tropism(const std::array<Bitboard, 6>& W, const std::array<Bitboard, 6>& B) {
  int wK = lsb_i(W[5]);
  int bK = lsb_i(B[5]);
  if (wK < 0 || bK < 0) return 0;
  int sc = 0;
  auto add = [&](Bitboard bb, int target, int sign, int base) {
    Bitboard t = bb;
    while (t) {
      int s = lsb_i(t);
      t &= t - 1;
      int d = king_manhattan(s, target);
      sc += sign * std::max(0, base - 2 * d);
    }
  };
  add(W[1], bK, +1, 12);
  add(W[2], bK, +1, 10);
  add(W[3], bK, +1, 8);
  add(W[4], bK, +1, 6);
  add(B[1], wK, -1, 12);
  add(B[2], wK, -1, 10);
  add(B[3], wK, -1, 8);
  add(B[4], wK, -1, 6);
  return sc / 2;
}

// =============================================================================
// Development & piece blocking
// =============================================================================
static int development(const std::array<Bitboard, 6>& W, const std::array<Bitboard, 6>& B) {
  Bitboard wMin = W[1] | W[2];
  Bitboard bMin = B[1] | B[2];
  Bitboard wInit = sq_bb(Square(1)) | sq_bb(Square(6)) | sq_bb(Square(2)) | sq_bb(Square(5));
  Bitboard bInit = sq_bb(Square(57)) | sq_bb(Square(62)) | sq_bb(Square(58)) | sq_bb(Square(61));
  int dW = popcnt(wMin & wInit);
  int dB = popcnt(bMin & bInit);
  return (dB - dW) * 16;
}

static int piece_blocking(const std::array<Bitboard, 6>& W, const std::array<Bitboard, 6>& B) {
  int s = 0;
  Bitboard wc = (sq_bb(Square(2 << 3 | 2)) | sq_bb(Square(3 << 3 | 2)));
  Bitboard wd = (sq_bb(Square(2 << 3 | 3)) | sq_bb(Square(3 << 3 | 3)));
  if ((W[1] | W[2]) & (wc | wd)) s -= 6;
  Bitboard bc = (sq_bb(Square(5 << 3 | 5)) | sq_bb(Square(4 << 3 | 5)));
  Bitboard bd = (sq_bb(Square(5 << 3 | 4)) | sq_bb(Square(4 << 3 | 4)));
  if ((B[1] | B[2]) & (bc | bd)) s += 6;
  return s;
}

// =============================================================================
// Endgame scalers
// =============================================================================
static int endgame_scale(const std::array<Bitboard, 6>& W, const std::array<Bitboard, 6>& B) {
  bool onlyBishops =
      ((W[1] | W[3] | W[4] | B[1] | B[3] | B[4]) == 0) && popcnt(W[2]) == 1 && popcnt(B[2]) == 1;
  if (onlyBishops) {
    int wBsq = lsb_i(W[2]);
    int bBsq = lsb_i(B[2]);
    bool wLight = ((file_of(wBsq) + rank_of(wBsq)) & 1) != 0;
    bool bLight = ((file_of(bBsq) + rank_of(bBsq)) & 1) != 0;
    if (wLight != bLight) return OPP_BISHOPS_SCALE;
  }
  return 256;
}

// =============================================================================
// Extra: castles & center
// =============================================================================
static void castling_and_center(const std::array<Bitboard, 6>& W, const std::array<Bitboard, 6>& B,
                                int& mg_add, int& eg_add) {
  init_masks();
  int wK = lsb_i(W[5]);
  int bK = lsb_i(B[5]);
  bool queensOn = (W[4] | B[4]) != 0;
  auto center_penalty = [&](int ksq, bool white) {
    if (ksq < 0) return 0;
    bool centerBack = (ksq == 4 || ksq == 3 || ksq == 5);
    if (!centerBack) return 0;
    Bitboard fileE = M.file[4], fileD = M.file[3];
    Bitboard ownP = white ? W[0] : B[0];
    Bitboard oppP = white ? B[0] : W[0];
    int amp = 0;
    auto openish = [&](Bitboard f) {
      bool own = (f & ownP), opp = (f & oppP);
      if (!own && !opp) return 2;
      if (!own && opp) return 1;
      return 0;
    };
    amp += openish(fileD) + openish(fileE);
    int base = queensOn ? 36 : 12;
    return base + amp * 8;
  };
  auto castle_bonus = [&](int ksq) { return (ksq == 6 || ksq == 2) ? 28 : 0; };
  mg_add += castle_bonus(wK);
  mg_add -= castle_bonus(mirror_sq_black(bK));
  mg_add += center_penalty(bK, false);
  mg_add -= center_penalty(wK, true);
  eg_add += (castle_bonus(wK) / 2) - (castle_bonus(mirror_sq_black(bK)) / 2);
}

// =============================================================================
// Eval caches
// =============================================================================
constexpr size_t EVAL_BITS = 14, EVAL_SIZE = 1ULL << EVAL_BITS;
constexpr size_t PAWN_BITS = 12, PAWN_SIZE = 1ULL << PAWN_BITS;
struct EvalEntry {
  std::atomic<uint64_t> key{0};
  std::atomic<int32_t> score{0};
  std::atomic<uint32_t> age{0};
  std::atomic<uint32_t> seq{0};
};
struct PawnEntry {
  std::atomic<uint64_t> key{0};
  std::atomic<int32_t> pawn{0};
  std::atomic<uint32_t> age{0};
  std::atomic<uint32_t> seq{0};
};
struct Evaluator::Impl {
  std::array<EvalEntry, EVAL_SIZE> eval;
  std::array<PawnEntry, PAWN_SIZE> pawn;
  std::atomic<uint32_t> age{1};
};
Evaluator::Evaluator() noexcept {
  m_impl = new Impl();
}
Evaluator::~Evaluator() noexcept {
  delete m_impl;
}
void Evaluator::clearCaches() const noexcept {
  if (!m_impl) return;
  for (auto& e : m_impl->eval) {
    e.key.store(0);
    e.score.store(0);
    e.age.store(0);
    e.seq.store(0);
  }
  for (auto& p : m_impl->pawn) {
    p.key.store(0);
    p.pawn.store(0);
    p.age.store(0);
    p.seq.store(0);
  }
  m_impl->age.store(1);
}
static inline size_t idx_eval(uint64_t k) {
  return (size_t)k & (EVAL_SIZE - 1);
}
static inline size_t idx_pawn(uint64_t k) {
  return (size_t)k & (PAWN_SIZE - 1);
}

// =============================================================================
// Material/PST/Phase gather + material counts
// =============================================================================
static void material_phase_counts(const std::array<Bitboard, 6>& W,
                                  const std::array<Bitboard, 6>& B, int& mg, int& eg, int& phase,
                                  MaterialCounts& mc) {
  mg = eg = phase = 0;
  mc = MaterialCounts{};
  for (int pt = 0; pt < 6; ++pt) {
    Bitboard bb = W[pt];
    auto P = (PieceType)pt;
    while (bb) {
      int s = lsb_i(bb);
      bb &= bb - 1;
      mg += VAL_MG[pt] + pst_mg(P, s);
      eg += VAL_EG[pt] + pst_eg(P, s);
      phase += PHASE_W[pt];
      if (pt == 0)
        mc.P[0]++;
      else if (pt == 1)
        mc.N[0]++;
      else if (pt == 2)
        mc.B[0]++;
      else if (pt == 3)
        mc.R[0]++;
      else if (pt == 4)
        mc.Q[0]++;
    }
  }
  for (int pt = 0; pt < 6; ++pt) {
    Bitboard bb = B[pt];
    auto P = (PieceType)pt;
    while (bb) {
      int s = lsb_i(bb);
      bb &= bb - 1;
      int ms = mirror_sq_black(s);
      mg -= VAL_MG[pt] + pst_mg(P, ms);
      eg -= VAL_EG[pt] + pst_eg(P, ms);
      phase += PHASE_W[pt];
      if (pt == 0)
        mc.P[1]++;
      else if (pt == 1)
        mc.N[1]++;
      else if (pt == 2)
        mc.B[1]++;
      else if (pt == 3)
        mc.R[1]++;
      else if (pt == 4)
        mc.Q[1]++;
    }
  }
}

// =============================================================================
// evaluate() – white POV
// =============================================================================
int Evaluator::evaluate(model::Position& pos) const {
  init_masks();
  const Board& b = pos.getBoard();
  uint64_t key = (uint64_t)pos.hash();
  uint64_t pKey = (uint64_t)pos.getState().pawnKey;

  // probe eval cache
  {
    auto& e = m_impl->eval[idx_eval(key)];
    for (int i = 0; i < 2; ++i) {
      uint32_t s1 = e.seq.load(std::memory_order_acquire);
      if (s1 & 1u) continue;
      uint64_t k = e.key.load(std::memory_order_acquire);
      int32_t sc = e.score.load(std::memory_order_acquire);
      uint32_t s2 = e.seq.load(std::memory_order_acquire);
      if (s1 == s2 && !(s2 & 1u) && k == key) return sc;
      if (k != key) break;
    }
  }

  // bitboards
  std::array<Bitboard, 6> W{}, B{};
  for (int pt = 0; pt < 6; ++pt) {
    W[pt] = b.getPieces(Color::White, (PieceType)pt);
    B[pt] = b.getPieces(Color::Black, (PieceType)pt);
  }
  Bitboard occ = b.getAllPieces();
  Bitboard wocc = b.getPieces(Color::White);
  Bitboard bocc = b.getPieces(Color::Black);

  // material & pst & phase & counts
  MaterialCounts mc;
  int mg = 0, eg = 0, phase = 0;
  material_phase_counts(W, B, mg, eg, phase, mc);
  int curPhase = clampi(phase, 0, MAX_PHASE);

  // pawn cache
  int pawnScore = std::numeric_limits<int>::min();
  {
    auto& ps = m_impl->pawn[idx_pawn(pKey)];
    for (int i = 0; i < 2; ++i) {
      uint32_t s1 = ps.seq.load(std::memory_order_acquire);
      if (s1 & 1u) continue;
      uint64_t k = ps.key.load(std::memory_order_acquire);
      int32_t sc = ps.pawn.load(std::memory_order_acquire);
      uint32_t s2 = ps.seq.load(std::memory_order_acquire);
      if (s1 == s2 && !(s2 & 1u) && k == pKey) {
        pawnScore = sc;
        break;
      }
      if (k != pKey) break;
    }
  }
  int wK = lsb_i(W[5]), bK = lsb_i(B[5]);
  if (pawnScore == std::numeric_limits<int>::min()) {
    pawnScore = pawn_structure(W[0], B[0], wK, bK, occ);
    auto& ps = m_impl->pawn[idx_pawn(pKey)];
    uint32_t s0 = ps.seq.load(std::memory_order_relaxed);
    ps.seq.store(s0 | 1u, std::memory_order_release);
    ps.pawn.store(pawnScore, std::memory_order_relaxed);
    ps.key.store(pKey, std::memory_order_release);
    ps.age.store(m_impl->age.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
    ps.seq.store((s0 | 1u) + 1u, std::memory_order_release);
  }

  // mobility & attacks
  AttInfo att = mobility(occ, wocc, bocc, W, B);

  // threats
  int thr = threats(W, B, att.wAll, att.bAll, occ);

  // king safety
  int ksRaw = king_safety_raw(W, B, occ);
  int shelter = king_shelter_storm(W, B);

  // style & structure
  int bp = bishop_pair_term(W, B);
  int badB = bad_bishop(W, B);
  int outp = outposts_center(W, B);
  int rim = rim_knights(W, B);
  int ract = rook_activity(W, B, W[0], B[0]);
  int spc = space_term(W, B);
  int trop = king_tropism(W, B);
  int dev = development(W, B);
  int block = piece_blocking(W, B);

  // material imbalance
  int imb = material_imbalance(mc);

  // Queens present?
  bool queensOn = (W[4] | B[4]) != 0;
  int ksMG = ksRaw * (queensOn ? 120 : 55) / 100;  // [TIGHTER SCALE]
  int ksEG = ksRaw / 8;
  int shelterMG = shelter;
  int shelterEG = shelter / 4;

  // Mix into MG/EG buckets
  int mg_add = 0, eg_add = 0;
  mg_add += (pawnScore / 2);
  eg_add += pawnScore;

  mg_add += att.mg;
  eg_add += att.eg;

  mg_add += ksMG + shelterMG + ksMG / 3;
  eg_add += ksEG + shelterEG;

  mg_add += (thr * 3) / 2;
  eg_add += thr / 2;

  mg_add += bp + imb;
  eg_add += bp / 2 + imb / 2;

  mg_add += dev * std::min(curPhase, 12) / 12;
  eg_add += dev / 8;

  mg_add += rim + outp + ract + badB + spc + block + trop;
  eg_add += (rim / 2) + (outp / 2) + (ract / 3) + (badB / 3) + (spc / 4) + (block / 2) + trop / 3;

  // castles & center
  castling_and_center(W, B, mg_add, eg_add);

  mg += mg_add;
  eg += eg_add;

  // blend
  int mg_w = (curPhase * 256) / MAX_PHASE;
  int eg_w = 256 - mg_w;
  int score = ((mg * mg_w) + (eg * eg_w)) >> 8;

  // tempo
  bool wtm = (pos.getState().sideToMove == Color::White);
  int tempo = ((TEMPO_MG * mg_w) + (TEMPO_EG * eg_w)) >> 8;
  score += (wtm ? +tempo : -tempo);

  // endgame scaling
  int scale = endgame_scale(W, B);
  score = (score * scale) / 256;

  score = clampi(score, -MATE + 1, MATE - 1);

  // store eval
  uint32_t age = m_impl->age.fetch_add(1, std::memory_order_relaxed) + 1;
  if (!age) {
    m_impl->age.store(1);
    age = 1;
  }
  auto& e = m_impl->eval[idx_eval(key)];
  uint32_t s0 = e.seq.load(std::memory_order_relaxed);
  e.seq.store(s0 | 1u, std::memory_order_release);
  e.score.store(score, std::memory_order_relaxed);
  e.key.store(key, std::memory_order_release);
  e.age.store(age, std::memory_order_relaxed);
  e.seq.store((s0 | 1u) + 1u, std::memory_order_release);
  return score;
}

}  // namespace lilia::engine
