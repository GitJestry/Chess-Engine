#include "lilia/engine/eval.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <mutex>
#include <shared_mutex>

#include "lilia/model/core/bitboard.hpp"
#include "lilia/model/position.hpp"

using namespace lilia::core;
using namespace lilia::model;
using namespace lilia::model::bb;  // deine bitboard-helpers

namespace lilia::engine {

// ---------- Konfiguration & Konstanten ----------
constexpr int INF = std::numeric_limits<int>::max() / 4;
constexpr int MATE_SCORE = 100000;

// Basiswerte (Centipawns) MG / EG (kannst du tunen)
constexpr std::array<int, 6> PIECE_VALUE_MG = {100, 320, 330, 500, 900, 0};
constexpr std::array<int, 6> PIECE_VALUE_EG = {100, 320, 330, 500, 900, 0};
constexpr std::array<int, 6> PIECE_PHASE = {0, 1, 1, 2, 4, 0};

// Zusatz-Boni / Strafen (tunable)
constexpr int BISHOP_PAIR_BONUS = 50;
constexpr int ROOK_OPEN_FILE_BONUS = 20;
constexpr int ROOK_SEMI_OPEN_FILE_BONUS = 10;
constexpr int ROOK_ON_SEVENTH_BONUS = 30;
constexpr int KNIGHT_RIM_PENALTY = 15;
constexpr int DEVELOPMENT_PENALTY = 20;  // per undeveloped minor piece (opening)
constexpr int OUTPOST_KNIGHT_BONUS = 30;
constexpr int CENTER_CONTROL_BONUS = 6;  // per attacking piece into central squares
constexpr int CONNECTED_ROOKS_BONUS = 20;

// Kleine PSTs (Beispielwerte). Du kannst sie später durch bessere/tuned Werte ersetzen.
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

// ---------- Utility ----------
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

// Masks (lazy init)
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

    // file mask
    Bitboard fm = 0;
    for (int rr = 0; rr < 8; ++rr) fm |= sq_bb(static_cast<core::Square>((rr << 3) | f));
    masks.file_mask[sq] = fm;

    // adjacent files
    Bitboard adj = 0;
    if (f > 0)
      for (int rr = 0; rr < 8; ++rr) adj |= sq_bb(static_cast<core::Square>((rr << 3) | (f - 1)));
    if (f < 7)
      for (int rr = 0; rr < 8; ++rr) adj |= sq_bb(static_cast<core::Square>((rr << 3) | (f + 1)));
    masks.adjacent_files[sq] = adj;

    // passed pawn masks
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

    // pawn front span (same file)
    Bitboard span_w = 0;
    for (int rr = r + 1; rr < 8; ++rr) span_w |= sq_bb(static_cast<core::Square>((rr << 3) | f));
    masks.pawn_front_white[sq] = span_w;

    Bitboard span_b = 0;
    for (int rr = r - 1; rr >= 0; --rr) span_b |= sq_bb(static_cast<core::Square>((rr << 3) | f));
    masks.pawn_front_black[sq] = span_b;
  }
  masks.ready = true;
}

// ---------- Subkomponenten der Eval ----------

// Material + PST + Phase
static void material_pst_phase(const Board& b, int& mg_out, int& eg_out, int& phase_out) {
  init_masks_if_needed();
  mg_out = eg_out = 0;
  phase_out = 0;

  // White pieces
  for (int pt = 0; pt < 6; ++pt) {
    PieceType piece = static_cast<PieceType>(pt);
    Bitboard bs = b.pieces(core::Color::White, piece);
    while (bs) {
      int sq = lsb_i(bs);
      bs &= bs - 1;
      mg_out += PIECE_VALUE_MG[pt] + pst_mg_for(piece, sq);
      eg_out += PIECE_VALUE_EG[pt] + pst_mg_for(piece, sq);  // reuse mg pst for eg for simplicity
      phase_out += PIECE_PHASE[pt];
    }
  }

  // Black pieces (subtract; mirror PST)
  for (int pt = 0; pt < 6; ++pt) {
    PieceType piece = static_cast<PieceType>(pt);
    Bitboard bs = b.pieces(core::Color::Black, piece);
    while (bs) {
      int sq = lsb_i(bs);
      bs &= bs - 1;
      // mirror index for black in PST: 63 - sq
      int msq = 63 - sq;
      mg_out -= PIECE_VALUE_MG[pt] + pst_mg_for(piece, msq);
      eg_out -= PIECE_VALUE_EG[pt] + pst_mg_for(piece, msq);
      phase_out -= PIECE_PHASE[pt];
    }
  }
}

// Pawn structure: isolated, doubled, passed — returns score (positive -> White better)
static int pawn_structure(const Board& b) {
  init_masks_if_needed();
  int score = 0;
  Bitboard wp = b.pieces(core::Color::White, PieceType::Pawn);
  Bitboard bp = b.pieces(core::Color::Black, PieceType::Pawn);

  // Isolated and doubled
  for (int sq = 0; sq < 64; ++sq) {
    Bitboard sqb = sq_bb(static_cast<core::Square>(sq));
    if (wp & sqb) {
      // isolated: no white pawn on adjacent files
      if ((masks.adjacent_files[sq] & wp) == 0) score -= 15;
      // doubled: another pawn on same file
      if ((masks.file_mask[sq] & wp & ~sqb) != 0) score -= 8;
      // passed pawn:
      if ((masks.passed_white[sq] & bp) == 0) {
        int r = sq_rank(sq);
        score += 20 + r * 8;
      }
    }
    if (bp & sqb) {
      if ((masks.adjacent_files[sq] & bp) == 0) score += 15;
      if ((masks.file_mask[sq] & bp & ~sqb) != 0) score += 8;
      if ((masks.passed_black[sq] & wp) == 0) {
        int r = sq_rank(sq);
        score -= 20 + (7 - r) * 8;
      }
    }
  }

  return score;
}

// Mobility: cheap pseudo-legal attack counts (excludes legality checks)
static int mobility(const Board& b) {
  init_masks_if_needed();
  int sc = 0;
  Bitboard occ = b.allPieces();
  Bitboard wocc = b.pieces(Color::White);
  Bitboard bocc = b.pieces(Color::Black);

  // Knights
  Bitboard wn = b.pieces(Color::White, PieceType::Knight);
  while (wn) {
    int sq = lsb_i(wn);
    wn &= wn - 1;
    Bitboard a = knight_attacks_from(static_cast<core::Square>(sq)) & ~wocc;
    sc += (popcnt(a) - 4) * 5;
  }
  Bitboard bn = b.pieces(Color::Black, PieceType::Knight);
  while (bn) {
    int sq = lsb_i(bn);
    bn &= bn - 1;
    Bitboard a = knight_attacks_from(static_cast<core::Square>(sq)) & ~bocc;
    sc -= (popcnt(a) - 4) * 5;
  }

  // Bishops
  Bitboard wb = b.pieces(Color::White, PieceType::Bishop);
  while (wb) {
    int sq = lsb_i(wb);
    wb &= wb - 1;
    Bitboard a = bishop_attacks(static_cast<core::Square>(sq), occ) & ~wocc;
    sc += popcnt(a) * 3;
  }
  Bitboard bb = b.pieces(Color::Black, PieceType::Bishop);
  while (bb) {
    int sq = lsb_i(bb);
    bb &= bb - 1;
    Bitboard a = bishop_attacks(static_cast<core::Square>(sq), occ) & ~bocc;
    sc -= popcnt(a) * 3;
  }

  // Rooks
  Bitboard wr = b.pieces(Color::White, PieceType::Rook);
  while (wr) {
    int sq = lsb_i(wr);
    wr &= wr - 1;
    Bitboard a = rook_attacks(static_cast<core::Square>(sq), occ) & ~wocc;
    sc += popcnt(a) * 2;
  }
  Bitboard br = b.pieces(Color::Black, PieceType::Rook);
  while (br) {
    int sq = lsb_i(br);
    br &= br - 1;
    Bitboard a = rook_attacks(static_cast<core::Square>(sq), occ) & ~bocc;
    sc -= popcnt(a) * 2;
  }

  // Queens
  Bitboard wq = b.pieces(Color::White, PieceType::Queen);
  while (wq) {
    int sq = lsb_i(wq);
    wq &= wq - 1;
    Bitboard a = queen_attacks(static_cast<core::Square>(sq), occ) & ~wocc;
    sc += popcnt(a) * 1;
  }
  Bitboard bq = b.pieces(Color::Black, PieceType::Queen);
  while (bq) {
    int sq = lsb_i(bq);
    bq &= bq - 1;
    Bitboard a = queen_attacks(static_cast<core::Square>(sq), occ) & ~bocc;
    sc -= popcnt(a) * 1;
  }

  return sc;
}

// King safety (pawn shield + tropism)
static int king_safety(const Board& b) {
  int sc = 0;
  // find king squares
  Bitboard wk = b.pieces(Color::White, PieceType::King);
  Bitboard bk = b.pieces(Color::Black, PieceType::King);
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
    sc += popcnt(b.pieces(Color::White, PieceType::Pawn) & shield) * 10;
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
    sc -= popcnt(b.pieces(Color::Black, PieceType::Pawn) & shield) * 10;
  }

  // tropsim: enemy pieces near king increase danger
  auto tropism = [&](int king_sq, Color enemy_color) -> int {
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
    // knights
    Bitboard kn = b.pieces(enemy_color, PieceType::Knight);
    sum += popcnt(kn & area) * 25;
    // heavy pieces
    Bitboard heavy = b.pieces(enemy_color, PieceType::Queen) |
                     b.pieces(enemy_color, PieceType::Rook) |
                     b.pieces(enemy_color, PieceType::Bishop);
    sum += popcnt(heavy & area) * 12;
    return sum;
  };

  sc -= tropism(wksq, Color::Black);
  sc += tropism(bksq, Color::White);

  return sc;
}

// ----- Neue Subkomponenten: Development / Piece-Activity / Rooks, etc. -----

// bishop pair bonus
static int bishop_pair(const Board& b) {
  int score = 0;
  if (popcnt(b.pieces(Color::White, PieceType::Bishop)) >= 2) score += BISHOP_PAIR_BONUS;
  if (popcnt(b.pieces(Color::Black, PieceType::Bishop)) >= 2) score -= BISHOP_PAIR_BONUS;
  return score;
}

// development: penalize minors on starting squares in opening (phase sensitive)
static int development_score(const Board& b) {
  Bitboard white_minors =
      b.pieces(Color::White, PieceType::Knight) | b.pieces(Color::White, PieceType::Bishop);
  Bitboard black_minors =
      b.pieces(Color::Black, PieceType::Knight) | b.pieces(Color::Black, PieceType::Bishop);

  Bitboard white_initial =
      sq_bb(static_cast<core::Square>(1)) | sq_bb(static_cast<core::Square>(6)) |
      sq_bb(static_cast<core::Square>(2)) | sq_bb(static_cast<core::Square>(5));
  Bitboard black_initial =
      sq_bb(static_cast<core::Square>(57)) | sq_bb(static_cast<core::Square>(62)) |
      sq_bb(static_cast<core::Square>(58)) | sq_bb(static_cast<core::Square>(61));

  int white_undeveloped = popcnt(white_minors & white_initial);
  int black_undeveloped = popcnt(black_minors & black_initial);

  // Positive -> white advantage (fewer undeveloped white minors)
  return (black_undeveloped - white_undeveloped) * DEVELOPMENT_PENALTY;
}

// knight rim penalty (knights on file a/h)
static int knight_rim(const Board& b) {
  int score = 0;
  Bitboard a_mask = masks.file_mask[0];
  Bitboard h_mask = masks.file_mask[7];

  Bitboard wn = b.pieces(Color::White, PieceType::Knight);
  Bitboard bn = b.pieces(Color::Black, PieceType::Knight);

  score -= popcnt(wn & (a_mask | h_mask)) * KNIGHT_RIM_PENALTY;
  score += popcnt(bn & (a_mask | h_mask)) * KNIGHT_RIM_PENALTY;

  return score;
}

// outpost/center control: knights on central squares, and pieces attacking center
static int center_and_outposts(const Board& b) {
  Bitboard center = sq_bb(static_cast<core::Square>(27)) | sq_bb(static_cast<core::Square>(28)) |
                    sq_bb(static_cast<core::Square>(35)) | sq_bb(static_cast<core::Square>(36));
  int score = 0;

  score += popcnt(b.pieces(Color::White, PieceType::Knight) & center) * OUTPOST_KNIGHT_BONUS;
  score -= popcnt(b.pieces(Color::Black, PieceType::Knight) & center) * OUTPOST_KNIGHT_BONUS;

  Bitboard wn = b.pieces(Color::White, PieceType::Knight);
  while (wn) {
    int sq = lsb_i(wn);
    wn &= wn - 1;
    if (knight_attacks_from(static_cast<core::Square>(sq)) & center) score += CENTER_CONTROL_BONUS;
  }
  Bitboard bn = b.pieces(Color::Black, PieceType::Knight);
  while (bn) {
    int sq = lsb_i(bn);
    bn &= bn - 1;
    if (knight_attacks_from(static_cast<core::Square>(sq)) & center) score -= CENTER_CONTROL_BONUS;
  }

  Bitboard wb = b.pieces(Color::White, PieceType::Bishop);
  while (wb) {
    int sq = lsb_i(wb);
    wb &= wb - 1;
    Bitboard a = bishop_attacks(static_cast<core::Square>(sq), b.allPieces());
    if (a & center) score += CENTER_CONTROL_BONUS;
  }
  Bitboard bb = b.pieces(Color::Black, PieceType::Bishop);
  while (bb) {
    int sq = lsb_i(bb);
    bb &= bb - 1;
    Bitboard a = bishop_attacks(static_cast<core::Square>(sq), b.allPieces());
    if (a & center) score -= CENTER_CONTROL_BONUS;
  }

  Bitboard wq = b.pieces(Color::White, PieceType::Queen);
  while (wq) {
    int sq = lsb_i(wq);
    wq &= wq - 1;
    Bitboard a = queen_attacks(static_cast<core::Square>(sq), b.allPieces());
    if (a & center) score += CENTER_CONTROL_BONUS;
  }
  Bitboard bq = b.pieces(Color::Black, PieceType::Queen);
  while (bq) {
    int sq = lsb_i(bq);
    bq &= bq - 1;
    Bitboard a = queen_attacks(static_cast<core::Square>(sq), b.allPieces());
    if (a & center) score -= CENTER_CONTROL_BONUS;
  }

  return score;
}

// rook on open/semi-open file and rook on 7th detection + connected rooks
static int rook_activity(const Board& b) {
  int score = 0;
  Bitboard wr = b.pieces(Color::White, PieceType::Rook);
  Bitboard br = b.pieces(Color::Black, PieceType::Rook);
  Bitboard wp = b.pieces(Color::White, PieceType::Pawn);
  Bitboard bp = b.pieces(Color::Black, PieceType::Pawn);

  auto file_of_sq = [&](int sq) -> Bitboard { return masks.file_mask[sq]; };
  auto rank_of_sq = [&](int sq) -> int { return sq_rank(sq); };

  while (wr) {
    int sq = lsb_i(wr);
    wr &= wr - 1;
    Bitboard file = file_of_sq(sq);
    bool has_white_pawn = (file & wp) != 0;
    bool has_black_pawn = (file & bp) != 0;
    if (!has_white_pawn && !has_black_pawn)
      score += ROOK_OPEN_FILE_BONUS;
    else if (!has_white_pawn && has_black_pawn)
      score += ROOK_SEMI_OPEN_FILE_BONUS;
    if (rank_of_sq(sq) == 6) score += ROOK_ON_SEVENTH_BONUS;
  }

  while (br) {
    int sq = lsb_i(br);
    br &= br - 1;
    Bitboard file = file_of_sq(sq);
    bool has_white_pawn = (file & wp) != 0;
    bool has_black_pawn = (file & bp) != 0;
    if (!has_white_pawn && !has_black_pawn)
      score -= ROOK_OPEN_FILE_BONUS;
    else if (!has_black_pawn && has_white_pawn)
      score -= ROOK_SEMI_OPEN_FILE_BONUS;
    if (rank_of_sq(sq) == 1) score -= ROOK_ON_SEVENTH_BONUS;
  }

  Bitboard wr_all = b.pieces(Color::White, PieceType::Rook);
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

  Bitboard br_all = b.pieces(Color::Black, PieceType::Rook);
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

// ---------- Thread-safe caches (direct-mapped) ----------
constexpr size_t EVAL_CACHE_BITS = 14;  // 16k entries
constexpr size_t EVAL_CACHE_SIZE = 1ULL << EVAL_CACHE_BITS;
struct EvalEntry {
  Bitboard key = 0;
  int32_t score = 0;
  uint32_t age = 0;
};

constexpr size_t PAWN_CACHE_BITS = 12;  // 4k entries
constexpr size_t PAWN_CACHE_SIZE = 1ULL << PAWN_CACHE_BITS;
struct PawnEntry {
  Bitboard key = 0;
  int32_t pawn_score = 0;
  uint32_t age = 0;
};

struct Evaluator::Impl {
  std::array<EvalEntry, EVAL_CACHE_SIZE> eval_cache;
  std::array<PawnEntry, PAWN_CACHE_SIZE> pawn_cache;
  mutable std::shared_mutex cacheMutex;
  uint32_t global_age = 1;

  Impl() {
    // entries default-initialized
  }
  inline void incr_age() {
    ++global_age;
    if (global_age == 0) global_age = 1;
  }
};

// ---------- Impl lifecycle ----------
Evaluator::Evaluator() noexcept {
  m_impl = new Impl();
}
Evaluator::~Evaluator() noexcept {
  delete m_impl;
}

void Evaluator::clearCaches() const noexcept {
  if (!m_impl) return;
  std::unique_lock lock(m_impl->cacheMutex);
  for (auto& e : m_impl->eval_cache) e.key = 0;
  for (auto& p : m_impl->pawn_cache) p.key = 0;
  m_impl->global_age = 1;
}

// Index helpers
static inline size_t eval_index_from_key(Bitboard key) noexcept {
  return static_cast<size_t>(key) & (EVAL_CACHE_SIZE - 1);
}
static inline size_t pawn_index_from_key(Bitboard key) noexcept {
  return static_cast<size_t>(key) & (PAWN_CACHE_SIZE - 1);
}

// ---------- evaluate(Position) with thread-safe caches ----------
int Evaluator::evaluate(model::Position& pos) const {
  const Board& b = pos.board();

  // use engine zobrist as board key (fast)
  const Bitboard board_key = static_cast<Bitboard>(pos.hash());

  // pawn key: prefer explicit pawnKey in GameState; fallback to compute.
  Bitboard pawn_key = 0;
  // if GameState has pawnKey field (recommended), use it:
  pawn_key = pos.state().pawnKey;
  // 1) try eval cache (shared lock)
  {
    std::shared_lock lock(m_impl->cacheMutex);
    size_t ei = eval_index_from_key(board_key);
    const EvalEntry& ent = m_impl->eval_cache[ei];
    if (ent.key == board_key && ent.age == m_impl->global_age) {
      return ent.score;
    }
  }

  // 2) try pawn cache (shared lock) to avoid recomputing pawn structure
  int pawn_score = std::numeric_limits<int>::min();
  {
    std::shared_lock lock(m_impl->cacheMutex);
    size_t pi = pawn_index_from_key(pawn_key);
    const PawnEntry& pent = m_impl->pawn_cache[pi];
    if (pent.key == pawn_key) pawn_score = pent.pawn_score;
  }

  // --- slow path: compute eval without holding locks ---
  int mg = 0, eg = 0, phase = 0;
  material_pst_phase(b, mg, eg, phase);

  if (pawn_score == std::numeric_limits<int>::min()) {
    pawn_score = pawn_structure(b);
  }

  int mob = mobility(b);
  int ks = king_safety(b);

  // zusätzliche Features
  int bishop_pair_score = bishop_pair(b);
  int dev_score = development_score(b);
  int knight_rim_score = knight_rim(b);
  int outpost_center_score = center_and_outposts(b);
  int rook_act = rook_activity(b);

  int mg_add = pawn_score + mob + ks + bishop_pair_score + dev_score + knight_rim_score +
               outpost_center_score + rook_act;
  int eg_add = (pawn_score / 2) + (mob / 3) + (ks / 2) + (bishop_pair_score / 2) + (dev_score / 4) +
               (outpost_center_score / 2) + (rook_act / 3);

  mg += mg_add;
  eg += eg_add;

  // Phase-Normalisierung:
  int max_phase = 0;
  for (int pt = 0; pt < 6; ++pt) {
    max_phase += PIECE_PHASE[pt] * (popcnt(b.pieces(Color::White, static_cast<PieceType>(pt))) +
                                    popcnt(b.pieces(Color::Black, static_cast<PieceType>(pt))));
  }
  int phase_norm = (max_phase > 0) ? std::clamp(phase, -max_phase, max_phase) : 0;
  int mg_weight = (max_phase > 0) ? (phase_norm * 256 / max_phase) : 0;
  int eg_weight = 256 - mg_weight;

  int final_score = ((mg * mg_weight) + (eg * eg_weight)) >> 8;

  // store results under unique_lock (short critical section)
  {
    std::unique_lock lock(m_impl->cacheMutex);
    // pawn cache store
    size_t pi = pawn_index_from_key(pawn_key);
    PawnEntry& pent = m_impl->pawn_cache[pi];
    pent.key = pawn_key;
    pent.pawn_score = pawn_score;
    pent.age = m_impl->global_age;

    // eval cache store
    size_t ei = eval_index_from_key(board_key);
    EvalEntry& ent = m_impl->eval_cache[ei];
    ent.key = board_key;
    ent.score = final_score;
    ent.age = m_impl->global_age;

    // bump age to reduce pathological collisions over time
    m_impl->incr_age();
  }

  return final_score;
}

}  // namespace lilia::engine
