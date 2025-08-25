#include "lilia/engine/eval.hpp"

#include <array>
#include <cstdint>
#include <mutex>

#include "lilia/engine/engine.hpp"
#include "lilia/model/board.hpp"
#include "lilia/model/core/bitboard.hpp"
#include "lilia/model/position.hpp"

namespace lilia::engine {

// --- Tunable constants ---
static constexpr int ISOLATED_PAWN_PENALTY = 20;
static constexpr int DOUBLED_PAWN_PENALTY = 10;
static constexpr int BACKWARD_PAWN_PENALTY = 15;
static constexpr int CONNECTED_PASSED_BONUS = 10;
static constexpr std::array<int, 8> PASSED_PAWN_BONUS_ADVANCE = {0, 5, 10, 20, 40, 80, 120, 0};
static constexpr int BISHOP_PAIR_BONUS = 50;
static constexpr int BAD_BISHOP_PENALTY = 20;
static constexpr int KNIGHT_OUTPOST_BONUS = 20;
static constexpr int ROOK_OPEN_FILE_BONUS = 20;
static constexpr int ROOK_SEVENTH_BONUS = 20;
static constexpr int KING_PAWN_SHIELD_MISSING_PENALTY = 30;
static constexpr int QUEEN_CENTER_BONUS = 10;
static constexpr int ENDGAME_MATERIAL_THRESHOLD = 1500;
static constexpr int BACKRANK_MINOR_PENALTY = 30;

// --- Precomputed tables ---
static inline model::bb::Bitboard FILE_MASKS[8] = {
    0x0101010101010101ULL << 0, 0x0101010101010101ULL << 1, 0x0101010101010101ULL << 2,
    0x0101010101010101ULL << 3, 0x0101010101010101ULL << 4, 0x0101010101010101ULL << 5,
    0x0101010101010101ULL << 6, 0x0101010101010101ULL << 7};
static inline model::bb::Bitboard RANK_MASKS[8] = {
    0xFFULL << (0 * 8), 0xFFULL << (1 * 8), 0xFFULL << (2 * 8), 0xFFULL << (3 * 8),
    0xFFULL << (4 * 8), 0xFFULL << (5 * 8), 0xFFULL << (6 * 8), 0xFFULL << (7 * 8)};

static inline model::bb::Bitboard PASSED_WHITE[64];
static inline model::bb::Bitboard PASSED_BLACK[64];
static inline model::bb::Bitboard KING_SHIELD_WHITE[64];
static inline model::bb::Bitboard KING_SHIELD_BLACK[64];

static inline int PST_CACHE[2][6][64];

static std::once_flag g_tables_init_flag;

static inline int mirror_sq(int sq) {
  return 63 - sq;
}
static inline int center_distance_precomputed[64] = {0};

// --- Initialize tables ---
static void init_tables() {
  for (int sq = 0; sq < 64; ++sq) {
    int r = sq / 8, f = sq % 8;
    center_distance_precomputed[sq] = abs(r - 3) + abs(f - 3);

    // Passed pawns
    model::bb::Bitboard wmask = 0ULL, bmask = 0ULL;
    for (int rr = r + 1; rr < 8; ++rr)
      for (int ff = std::max(0, f - 1); ff <= std::min(7, f + 1); ++ff)
        wmask |= 1ULL << (rr * 8 + ff);
    for (int rr = r - 1; rr >= 0; --rr)
      for (int ff = std::max(0, f - 1); ff <= std::min(7, f + 1); ++ff)
        bmask |= 1ULL << (rr * 8 + ff);
    PASSED_WHITE[sq] = wmask;
    PASSED_BLACK[sq] = bmask;

    // King shield
    int kr = r, kf = f;
    model::bb::Bitboard wshield = 0ULL, bshield = 0ULL;
    for (int rr = kr + 1; rr <= std::min(7, kr + 2); ++rr)
      for (int ff = std::max(0, kf - 1); ff <= std::min(7, kf + 1); ++ff)
        wshield |= 1ULL << (rr * 8 + ff);
    for (int rr = std::max(0, kr - 2); rr <= kr - 1; ++rr)
      for (int ff = std::max(0, kf - 1); ff <= std::min(7, kf + 1); ++ff)
        bshield |= 1ULL << (rr * 8 + ff);
    KING_SHIELD_WHITE[sq] = wshield;
    KING_SHIELD_BLACK[sq] = bshield;
  }

  // PST_CACHE
  for (int isEG = 0; isEG <= 1; ++isEG)
    for (int p = 0; p < 6; ++p)
      for (int sq = 0; sq < 64; ++sq) {
        int cd = center_distance_precomputed[sq], val = 0;
        switch (p) {
          case 0:
            val = 4 - cd;
            break;  // pawn
          case 1:
            val = 48 - 6 * cd;
            break;  // knight
          case 2:
            val = 32 - 4 * cd;
            break;  // bishop
          case 3:
            val = 8 - cd;
            break;  // rook
          case 4:
            val = 10 - cd;
            break;  // queen
          case 5:
            val = isEG ? 20 - 2 * cd : -10 - cd;
            break;  // king
        }
        PST_CACHE[isEG][p][sq] = val;
      }
}

Evaluator::Evaluator() {
  std::call_once(g_tables_init_flag, init_tables);
  for (int c = 0; c < 2; ++c)
    for (int p = 0; p < 6; ++p)
      for (int sq = 0; sq < 64; ++sq) pst[c][p][sq] = 0;
}

int Evaluator::evaluate(model::Position& pos) const {
  using namespace model::bb;
  static constexpr int MATE_SCORE = 100000;

  if (pos.checkNoLegalMoves()) return pos.inCheck() ? MATE_SCORE : 0;
  const model::Board& b = pos.board();

  // --- Bitboards ---
  model::bb::Bitboard wP = b.pieces(core::Color::White, core::PieceType::Pawn);
  model::bb::Bitboard wN = b.pieces(core::Color::White, core::PieceType::Knight);
  model::bb::Bitboard wB = b.pieces(core::Color::White, core::PieceType::Bishop);
  model::bb::Bitboard wR = b.pieces(core::Color::White, core::PieceType::Rook);
  model::bb::Bitboard wQ = b.pieces(core::Color::White, core::PieceType::Queen);
  model::bb::Bitboard wK = b.pieces(core::Color::White, core::PieceType::King);

  model::bb::Bitboard bP = b.pieces(core::Color::Black, core::PieceType::Pawn);
  model::bb::Bitboard bN = b.pieces(core::Color::Black, core::PieceType::Knight);
  model::bb::Bitboard bB = b.pieces(core::Color::Black, core::PieceType::Bishop);
  model::bb::Bitboard bR = b.pieces(core::Color::Black, core::PieceType::Rook);
  model::bb::Bitboard bQ = b.pieces(core::Color::Black, core::PieceType::Queen);
  model::bb::Bitboard bK = b.pieces(core::Color::Black, core::PieceType::King);

  int score = 0;

  // --- Material & Endgame ---
  int wMat = popcount(wP) * base_value[0] + popcount(wN) * base_value[1] +
             popcount(wB) * base_value[2] + popcount(wR) * base_value[3] +
             popcount(wQ) * base_value[4];
  int bMat = popcount(bP) * base_value[0] + popcount(bN) * base_value[1] +
             popcount(bB) * base_value[2] + popcount(bR) * base_value[3] +
             popcount(bQ) * base_value[4];
  score += wMat - bMat;
  bool isEG = (wMat + bMat) < ENDGAME_MATERIAL_THRESHOLD;
  int eg_index = isEG ? 1 : 0;

  // --- Piece-square & piece bonuses ---
  auto eval_pieces = [&](model::bb::Bitboard bb, int pt, int sign) {
    while (any(bb)) {
      int sq = ctz64(bb & -bb);
      bb &= bb - 1;
      score += sign * PST_CACHE[eg_index][pt][sign > 0 ? sq : mirror_sq(sq)];
      int r = sq / 8;
      if ((pt == 1 || pt == 2) && ((sign > 0 && r == 0) || (sign < 0 && r == 7)))
        score -= sign * BACKRANK_MINOR_PENALTY;
      if (pt == 1 && ((sign > 0 && r >= 3 && r <= 5) || (sign < 0 && r >= 2 && r <= 4)))
        score += sign * KNIGHT_OUTPOST_BONUS;
      if (pt == 2 && popcount(bb) >= 1) score += sign * BISHOP_PAIR_BONUS / 2;
      if (pt == 4 && r >= 2 && r <= 5) score += sign * QUEEN_CENTER_BONUS;
    }
  };
  eval_pieces(wN, 1, +1);
  eval_pieces(wB, 2, +1);
  eval_pieces(wR, 3, +1);
  eval_pieces(wQ, 4, +1);
  eval_pieces(bN, 1, -1);
  eval_pieces(bB, 2, -1);
  eval_pieces(bR, 3, -1);
  eval_pieces(bQ, 4, -1);

  // --- Pawns: doubled, isolated, backward, connected, passed ---
  for (int f = 0; f < 8; ++f) {
    int wc = popcount(wP & FILE_MASKS[f]);
    int bc = popcount(bP & FILE_MASKS[f]);
    if (wc > 1) score -= DOUBLED_PAWN_PENALTY * (wc - 1);
    if (bc > 1) score += DOUBLED_PAWN_PENALTY * (bc - 1);
  }

  auto eval_pawns = [&](model::bb::Bitboard P, model::bb::Bitboard ownP, model::bb::Bitboard oppP,
                        int sign, bool isWhite) {
    while (any(P)) {
      int sq = ctz64(P & -P);
      P &= P - 1;
      int f = sq % 8, r = sq / 8;

      // Isolated
      model::bb::Bitboard adj = 0;
      if (f > 0) adj |= FILE_MASKS[f - 1];
      if (f < 7) adj |= FILE_MASKS[f + 1];
      if ((ownP & adj) == 0ULL) score -= sign * ISOLATED_PAWN_PENALTY;

      // Backward
      model::bb::Bitboard support = 0ULL;
      if (isWhite) {
        if (f > 0) support |= 1ULL << ((r - 1) * 8 + f - 1);
        if (f < 7) support |= 1ULL << ((r - 1) * 8 + f + 1);
      } else {
        if (f > 0) support |= 1ULL << ((r + 1) * 8 + f - 1);
        if (f < 7) support |= 1ULL << ((r + 1) * 8 + f + 1);
      }
      if ((ownP & support) == 0) score -= sign * BACKWARD_PAWN_PENALTY;

      // Connected
      if ((ownP & support) != 0) score += sign * CONNECTED_PASSED_BONUS;

      // Passed
      if ((oppP & (isWhite ? PASSED_WHITE[sq] : PASSED_BLACK[sq])) == 0ULL) {
        score += sign * PASSED_PAWN_BONUS_ADVANCE[isWhite ? r : 7 - r];
      }

      // PST
      score += sign * PST_CACHE[eg_index][0][isWhite ? sq : mirror_sq(sq)];
    }
  };
  eval_pawns(wP, wP, bP, +1, true);
  eval_pawns(bP, bP, wP, -1, false);

  // --- Rooks ---
  auto eval_rooks = [&](model::bb::Bitboard R, model::bb::Bitboard ownP, model::bb::Bitboard oppP,
                        int sign) {
    while (any(R)) {
      int sq = ctz64(R & -R);
      R &= R - 1;
      int f = sq % 8, r = sq / 8;
      model::bb::Bitboard mask = FILE_MASKS[f];
      if ((ownP & mask) == 0ULL && (oppP & mask) == 0ULL)
        score += sign * ROOK_OPEN_FILE_BONUS;
      else if ((ownP & mask) == 0ULL)
        score += sign * (ROOK_OPEN_FILE_BONUS / 2);
      if ((sign > 0 && r == 6) || (sign < 0 && r == 1)) score += sign * ROOK_SEVENTH_BONUS;
    }
  };
  eval_rooks(wR, wP, bP, +1);
  eval_rooks(bR, bP, wP, -1);

  // --- King pawn shield ---
  auto king_shield = [&](model::bb::Bitboard K, model::bb::Bitboard ownP, bool isWhite, int sign) {
    if (!any(K)) return;
    int ksq = ctz64(K);
    model::bb::Bitboard mask = isWhite ? KING_SHIELD_WHITE[ksq] : KING_SHIELD_BLACK[ksq];
    int missing = popcount(mask) - popcount(ownP & mask);
    score -= sign * ((missing * KING_PAWN_SHIELD_MISSING_PENALTY) / 6);
  };
  king_shield(wK, wP, true, +1);
  king_shield(bK, bP, false, -1);

  // --- King attacks (open/semi-open files) ---
  auto king_attack = [&](model::bb::Bitboard K, model::bb::Bitboard enemyR,
                         model::bb::Bitboard enemyQ, model::bb::Bitboard P, bool isWhite,
                         int sign) {
    if (!any(K)) return;
    int ksq = ctz64(K);
    int kf = ksq % 8;
    model::bb::Bitboard file = FILE_MASKS[kf], semi_open = file & ~P;
    if ((enemyR & semi_open) != 0ULL || (enemyQ & semi_open) != 0ULL)
      score -= sign * ROOK_OPEN_FILE_BONUS;
  };
  king_attack(wK, bR, bQ, bP, true, +1);
  king_attack(bK, wR, wQ, wP, false, -1);

  return score;
}

}  // namespace lilia
