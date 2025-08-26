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
constexpr int MATE_SCORE = 100000;

constexpr std::array<int, 6> PIECE_VALUE_MG = {100, 320, 330, 500, 900, 0};
constexpr std::array<int, 6> PIECE_VALUE_EG = {100, 320, 330, 500, 900, 0};
constexpr std::array<int, 6> PIECE_PHASE = {0, 1, 1, 2, 4, 0};

constexpr int BISHOP_PAIR_BONUS = 50;
constexpr int ROOK_OPEN_FILE_BONUS = 20;
constexpr int ROOK_SEMI_OPEN_FILE_BONUS = 10;
constexpr int ROOK_ON_SEVENTH_BONUS = 30;
constexpr int KNIGHT_RIM_PENALTY = 15;
constexpr int DEVELOPMENT_PENALTY = 20;
constexpr int OUTPOST_KNIGHT_BONUS = 30;
constexpr int CENTER_CONTROL_BONUS = 6;
constexpr int CONNECTED_ROOKS_BONUS = 20;

static constexpr std::array<int, 64> PST_P_MG = {
    0,  0,  0,  0,  0,  0,  0,  0,  5,  10, 10, -20, -20, 10, 10, 5,  5, -5, -10, 0,  0,  -10,
    -5, 5,  0,  0,  0,  20, 20, 0,  0,  0,  5,  5,   10,  25, 25, 10, 5, 5,  10,  10, 20, 30,
    30, 20, 10, 10, 50, 50, 50, 50, 50, 50, 50, 50,  0,   0,  0,  0,  0, 0,  0,   0};
static constexpr std::array<int, 64> PST_N_MG = {
    -50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0,   0,   0,   0,   -20, -40,
    -30, 0,   10,  15,  15,  10,  0,   -30, -30, 5,   15,  20,  20,  15,  5,   -30,
    -30, 0,   15,  20,  20,  15,  0,   -30, -30, 5,   10,  15,  15,  10,  5,   -30,
    -40, -20, 0,   5,   5,   0,   -20, -40, -50, -40, -30, -30, -30, -30, -40, -50};
static constexpr std::array<int, 64> PST_B_MG = PST_N_MG;
static constexpr std::array<int, 64> PST_R_MG = {
    0, 0,  0,  5,  5, 0,  0,  0,  -5, 0,  0,  0, 0, 0, 0, -5, -5, 0,  0,  0, 0, 0,
    0, -5, -5, 0,  0, 0,  0,  0,  0,  -5, -5, 0, 0, 0, 0, 0,  0,  -5, -5, 0, 0, 0,
    0, 0,  0,  -5, 5, 10, 10, 10, 10, 10, 10, 5, 0, 0, 0, 0,  0,  0,  0,  0};
static constexpr std::array<int, 64> PST_Q_MG = PST_N_MG;
static constexpr std::array<int, 64> PST_K_MG = {
    -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30,
    -20, -30, -30, -40, -40, -30, -30, -20, -10, -20, -20, -20, -20, -20, -20, -10,
    20,  20,  0,   0,   0,   0,   20,  20,  20,  30,  10,  0,   0,   10,  30,  20};

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
  bool ready = false;
} masks;

static void init_masks_if_needed() {
  if (masks.ready) return;
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
  masks.ready = true;
}

static void material_pst_phase(const std::array<Bitboard, 6>& wbbs,
                               const std::array<Bitboard, 6>& bbbs, int& mg_out, int& eg_out,
                               int& phase_out) {
  mg_out = eg_out = 0;
  phase_out = 0;
  for (int pt = 0; pt < 6; ++pt) {
    PieceType piece = static_cast<PieceType>(pt);
    Bitboard wbs = wbbs[pt];
    while (wbs) {
      int sq = lsb_i(wbs);
      wbs &= wbs - 1;
      mg_out += PIECE_VALUE_MG[pt] + pst_mg_for(piece, sq);
      eg_out += PIECE_VALUE_EG[pt] + pst_mg_for(piece, sq);
      phase_out += PIECE_PHASE[pt];
    }
  }
  for (int pt = 0; pt < 6; ++pt) {
    PieceType piece = static_cast<PieceType>(pt);
    Bitboard bbs = bbbs[pt];
    while (bbs) {
      int sq = lsb_i(bbs);
      bbs &= bbs - 1;
      int msq = 63 - sq;
      mg_out -= PIECE_VALUE_MG[pt] + pst_mg_for(piece, msq);
      eg_out -= PIECE_VALUE_EG[pt] + pst_mg_for(piece, msq);
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
    if ((masks.adjacent_files[sq] & wp_orig) == 0) score -= 15;
    if ((masks.file_mask[sq] & wp_orig & ~sq_bb(static_cast<core::Square>(sq))) != 0) score -= 8;
    if ((masks.passed_white[sq] & bp_orig) == 0) {
      int r = sq_rank(sq);
      score += 20 + r * 8;
    }
  }
  while (bp) {
    int sq = lsb_i(bp);
    bp &= bp - 1;
    if ((masks.adjacent_files[sq] & bp_orig) == 0) score += 15;
    if ((masks.file_mask[sq] & bp_orig & ~sq_bb(static_cast<core::Square>(sq))) != 0) score += 8;
    if ((masks.passed_black[sq] & wp_orig) == 0) {
      int r = sq_rank(sq);
      score -= 20 + (7 - r) * 8;
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
        (magic::sliding_attacks(magic::Slider::Rook, static_cast<core::Square>(sq), occ) ||
         magic::sliding_attacks(magic::Slider::Bishop, static_cast<core::Square>(sq), occ)) &
        ~wocc;
    sc += popcnt(a) * 1;
  }
  Bitboard bq = bbbs[static_cast<int>(PieceType::Queen)];
  while (bq) {
    int sq = lsb_i(bq);
    bq &= bq - 1;
    Bitboard a =
        (magic::sliding_attacks(magic::Slider::Rook, static_cast<core::Square>(sq), occ) ||
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
  Bitboard center = sq_bb(static_cast<core::Square>(27)) | sq_bb(static_cast<core::Square>(28)) |
                    sq_bb(static_cast<core::Square>(35)) | sq_bb(static_cast<core::Square>(36));
  int score = 0;

  score += popcnt(wbbs[static_cast<int>(PieceType::Knight)] & center) * OUTPOST_KNIGHT_BONUS;
  score -= popcnt(bbbs[static_cast<int>(PieceType::Knight)] & center) * OUTPOST_KNIGHT_BONUS;

  Bitboard wn = wbbs[static_cast<int>(PieceType::Knight)];
  while (wn) {
    int sq = lsb_i(wn);
    wn &= wn - 1;
    if (knight_attacks_from(static_cast<core::Square>(sq)) & center) score += CENTER_CONTROL_BONUS;
  }
  Bitboard bn = bbbs[static_cast<int>(PieceType::Knight)];
  while (bn) {
    int sq = lsb_i(bn);
    bn &= bn - 1;
    if (knight_attacks_from(static_cast<core::Square>(sq)) & center) score -= CENTER_CONTROL_BONUS;
  }

  Bitboard wb = wbbs[static_cast<int>(PieceType::Bishop)];
  while (wb) {
    int sq = lsb_i(wb);
    wb &= wb - 1;
    Bitboard a = magic::sliding_attacks(magic::Slider::Bishop, static_cast<core::Square>(sq), occ);
    if (a & center) score += CENTER_CONTROL_BONUS;
  }
  Bitboard bb = bbbs[static_cast<int>(PieceType::Bishop)];
  while (bb) {
    int sq = lsb_i(bb);
    bb &= bb - 1;
    Bitboard a = magic::sliding_attacks(magic::Slider::Bishop, static_cast<core::Square>(sq), occ);
    if (a & center) score -= CENTER_CONTROL_BONUS;
  }

  Bitboard wq = wbbs[static_cast<int>(PieceType::Queen)];
  while (wq) {
    int sq = lsb_i(wq);
    wq &= wq - 1;
    Bitboard a =
        (magic::sliding_attacks(magic::Slider::Rook, static_cast<core::Square>(sq), occ) ||
         magic::sliding_attacks(magic::Slider::Bishop, static_cast<core::Square>(sq), occ));
    if (a & center) score += CENTER_CONTROL_BONUS;
  }
  Bitboard bq = bbbs[static_cast<int>(PieceType::Queen)];
  while (bq) {
    int sq = lsb_i(bq);
    bq &= bq - 1;
    Bitboard a =
        (magic::sliding_attacks(magic::Slider::Rook, static_cast<core::Square>(sq), occ) ||
         magic::sliding_attacks(magic::Slider::Bishop, static_cast<core::Square>(sq), occ));
    if (a & center) score -= CENTER_CONTROL_BONUS;
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
struct EvalEntry {
  std::atomic<uint64_t> key{0};
  std::atomic<int32_t> score{0};
  std::atomic<uint32_t> age{0};
};

constexpr size_t PAWN_CACHE_BITS = 12;  
constexpr size_t PAWN_CACHE_SIZE = 1ULL << PAWN_CACHE_BITS;
struct PawnEntry {
  std::atomic<uint64_t> key{0};
  std::atomic<int32_t> pawn_score{0};
  std::atomic<uint32_t> age{0};
};

struct Evaluator::Impl {
  std::array<EvalEntry, EVAL_CACHE_SIZE> eval_cache;
  std::array<PawnEntry, PAWN_CACHE_SIZE> pawn_cache;
  std::mutex writeMutex;  
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
  std::lock_guard lock(m_impl->writeMutex);
  for (auto& e : m_impl->eval_cache) {
    e.key.store(0, std::memory_order_relaxed);
    e.score.store(0, std::memory_order_relaxed);
    e.age.store(0, std::memory_order_relaxed);
  }
  for (auto& p : m_impl->pawn_cache) {
    p.key.store(0, std::memory_order_relaxed);
    p.pawn_score.store(0, std::memory_order_relaxed);
    p.age.store(0, std::memory_order_relaxed);
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
  const Board& b = pos.board();
  const uint64_t board_key = static_cast<uint64_t>(pos.hash());
  const uint64_t pawn_key = static_cast<uint64_t>(pos.state().pawnKey);

  
  {
    size_t ei = eval_index_from_key(board_key);
    uint64_t k = m_impl->eval_cache[ei].key.load(std::memory_order_relaxed);
    if (k == board_key) {
      int32_t s = m_impl->eval_cache[ei].score.load(std::memory_order_relaxed);
      return s;
    }
  }

  
  int pawn_score = std::numeric_limits<int>::min();
  {
    size_t pi = pawn_index_from_key(pawn_key);
    uint64_t pk = m_impl->pawn_cache[pi].key.load(std::memory_order_relaxed);
    if (pk == pawn_key)
      pawn_score = m_impl->pawn_cache[pi].pawn_score.load(std::memory_order_relaxed);
  }

  
  std::array<Bitboard, 6> wbbs{}, bbbs{};
  for (int pt = 0; pt < 6; ++pt) {
    wbbs[pt] = b.pieces(Color::White, static_cast<PieceType>(pt));
    bbbs[pt] = b.pieces(Color::Black, static_cast<PieceType>(pt));
  }
  Bitboard occ = b.allPieces();
  Bitboard wocc = b.pieces(Color::White);
  Bitboard bocc = b.pieces(Color::Black);

  int mg = 0, eg = 0, phase = 0;
  material_pst_phase(wbbs, bbbs, mg, eg, phase);

  if (pawn_score == std::numeric_limits<int>::min()) {
    pawn_score = pawn_structure(wbbs[static_cast<int>(PieceType::Pawn)],
                                bbbs[static_cast<int>(PieceType::Pawn)]);
  }

  int mob = mobility(occ, wocc, bocc, wbbs, bbbs);
  int ks = king_safety(wbbs, bbbs, occ);

  int bishop_pair_score = bishop_pair(wbbs, bbbs);
  int dev_score = development_score(wbbs, bbbs);
  int knight_rim_score = knight_rim(wbbs, bbbs);
  int outpost_center_score = center_and_outposts(wbbs, bbbs, occ);
  int rook_act = rook_activity(wbbs, bbbs, wbbs[static_cast<int>(PieceType::Pawn)],
                               bbbs[static_cast<int>(PieceType::Pawn)]);

  int mg_add = pawn_score + mob + ks + bishop_pair_score + dev_score + knight_rim_score +
               outpost_center_score + rook_act;
  int eg_add = (pawn_score / 2) + (mob / 3) + (ks / 2) + (bishop_pair_score / 2) + (dev_score / 4) +
               (outpost_center_score / 2) + (rook_act / 3);

  mg += mg_add;
  eg += eg_add;

  
  int max_phase = 0;
  for (int pt = 0; pt < 6; ++pt) {
    max_phase += PIECE_PHASE[pt] * (popcnt(wbbs[pt]) + popcnt(bbbs[pt]));
  }
  int phase_norm = (max_phase > 0) ? std::clamp(phase, 0, max_phase) : 0;
  int mg_weight = (max_phase > 0) ? (phase_norm * 256 / max_phase) : 0;
  int eg_weight = 256 - mg_weight;

  int final_score = ((mg * mg_weight) + (eg * eg_weight)) >> 8;

  
  {
    std::lock_guard<std::mutex> lock(m_impl->writeMutex);
    
    size_t pi = pawn_index_from_key(pawn_key);
    m_impl->pawn_cache[pi].key.store(pawn_key, std::memory_order_relaxed);
    m_impl->pawn_cache[pi].pawn_score.store(pawn_score, std::memory_order_relaxed);
    m_impl->pawn_cache[pi].age.store(m_impl->global_age.load(std::memory_order_relaxed),
                                     std::memory_order_relaxed);

    
    size_t ei = eval_index_from_key(board_key);
    m_impl->eval_cache[ei].key.store(board_key, std::memory_order_relaxed);
    m_impl->eval_cache[ei].score.store(final_score, std::memory_order_relaxed);
    m_impl->eval_cache[ei].age.store(m_impl->global_age.load(std::memory_order_relaxed),
                                     std::memory_order_relaxed);

    m_impl->incr_age();
  }

  return final_score;
}

}  
