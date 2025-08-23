// FILE: src/eval.cpp
#include "lilia/engine/eval.hpp"

#include <array>
#include <cstdint>

#include "lilia/engine/engine.hpp"
#include "lilia/model/board.hpp"
#include "lilia/model/core/bitboard.hpp"  // falls du helpers anders nennst, anpassen
#include "lilia/model/position.hpp"

namespace lilia {

// tuning constants (easy to change)
static const int ISOLATED_PAWN_PENALTY = 20;
static const int DOUBLED_PAWN_PENALTY = 10;
static const int PASSED_PAWN_BONUS_ADVANCE[8] = {0,  5,  10,  20,
                                                 40, 80, 120, 0};  // index = rank (0..7) for white
static const int BISHOP_PAIR_BONUS = 50;
static const int BACKRANK_MINOR_PENALTY = 30;
static const int ROOK_OPEN_FILE_BONUS = 20;
static const int KING_PAWN_SHIELD_MISSING_PENALTY = 30;

Evaluator::Evaluator() {
  // Nothing to precompute here; PST calculated on the fly by small formulas.
  // Keep PST array to satisfy header signature (not used heavily).
  for (int c = 0; c < 2; ++c)
    for (int p = 0; p < 6; ++p)
      for (int sq = 0; sq < 64; ++sq) pst[c][p][sq] = 0;
}

// helper: file mask and rank mask
static inline uint64_t file_mask(int f) {
  return 0x0101010101010101ULL << f;
}
static inline uint64_t rank_mask(int r) {
  return 0xFFULL << (r * 8);
}

// mirror square vertically (for black viewpoint)
static inline int mirror_sq(int sq) {
  return (63 - sq);
}

// small utility: distance-from-center as integer metric (0 best)
static inline int center_distance(int sq) {
  int r = sq / 8;
  int f = sq % 8;
  int dr = (r > 3) ? (r - 3) : (3 - r);
  int df = (f > 3) ? (f - 3) : (3 - f);
  return dr + df;
}

// piece-square-ish bonus generator (cheap)
static inline int piece_square_bonus(int pieceType, int sq, bool isWhite, bool isEndgame) {
  // pieceType: 0..5  (P,N,B,R,Q,K)
  // produce midgame-style bonuses:
  int cd = center_distance(isWhite ? sq : mirror_sq(sq));
  switch (pieceType) {
    case 0:                // pawn: small central bonus and advancement handled separately
      return 4 - cd;       // small
    case 1:                // knight: strong center preference
      return 48 - 6 * cd;  // example: center ~48, edge negative-ish
    case 2:                // bishop: center & long diagonals (use center distance)
      return 32 - 4 * cd;
    case 3:  // rook: prefer open files (handled separately), slight centralization
      return 8 - 1 * cd;
    case 4:  // queen: mobility/centralization
      return 10 - 1 * cd;
    case 5:  // king: in endgame prefer center, in mg prefer safety (we'll penalize later)
      return isEndgame ? (20 - 2 * cd) : (-10 - cd);
    default:
      return 0;
  }
}

// count pawns on a file
static inline int pawns_on_file(uint64_t pawns, int file) {
  uint64_t mask = file_mask(file);
  return model::bb::popcount(pawns & mask);
}

int Evaluator::evaluate(const model::Position& pos) const {
  // phase calculation for simple tapered eval: heavier pieces contribute to opening/mg
  // phase: 0..24 for example (higher -> early game)
  const model::Board& b = pos.board();
  uint64_t wP = b.pieces(core::Color::White, core::PieceType::Pawn);
  uint64_t wN = b.pieces(core::Color::White, core::PieceType::Knight);
  uint64_t wB = b.pieces(core::Color::White, core::PieceType::Bishop);
  uint64_t wR = b.pieces(core::Color::White, core::PieceType::Rook);
  uint64_t wQ = b.pieces(core::Color::White, core::PieceType::Queen);
  uint64_t wK = b.pieces(core::Color::White, core::PieceType::King);

  uint64_t bP = b.pieces(core::Color::Black, core::PieceType::Pawn);
  uint64_t bN = b.pieces(core::Color::Black, core::PieceType::Knight);
  uint64_t bB = b.pieces(core::Color::Black, core::PieceType::Bishop);
  uint64_t bR = b.pieces(core::Color::Black, core::PieceType::Rook);
  uint64_t bQ = b.pieces(core::Color::Black, core::PieceType::Queen);
  uint64_t bK = b.pieces(core::Color::Black, core::PieceType::King);

  int score = 0;

  // Material + PST-ish center/development bonuses
  auto add_piece_list = [&](uint64_t bb, int pieceType, int colorSign) {
    while (bb) {
      uint64_t lsb = bb & -bb;
      int sq = model::bb::ctz64(bb);
      bb -= lsb;
      int val = base_value[pieceType];
      // simple phase heuristic: endgame when heavy pieces removed
      // compute on the fly: endgame if both queens gone and minor pieces reduced (cheap
      // approximate) We'll compute a small boolean below; for now assume not endgame and compute
      // endgame after material sum PST-like bonus applied later (we'll add after endgame detection)
      score += colorSign * val;
    }
  };

  // compute material sums to determine endgame
  int wMaterial =
      (int)model::bb::popcount(wP) * base_value[0] + (int)model::bb::popcount(wN) * base_value[1] +
      (int)model::bb::popcount(wB) * base_value[2] + (int)model::bb::popcount(wR) * base_value[3] +
      (int)model::bb::popcount(wQ) * base_value[4];
  int bMaterial =
      (int)model::bb::popcount(bP) * base_value[0] + (int)model::bb::popcount(bN) * base_value[1] +
      (int)model::bb::popcount(bB) * base_value[2] + (int)model::bb::popcount(bR) * base_value[3] +
      (int)model::bb::popcount(bQ) * base_value[4];

  bool isEndgame = (wMaterial < 1500 && bMaterial < 1500);  // tunable threshold

  // Material base
  score += (int)model::bb::popcount(wP) * base_value[0];
  score += (int)model::bb::popcount(wN) * base_value[1];
  score += (int)model::bb::popcount(wB) * base_value[2];
  score += (int)model::bb::popcount(wR) * base_value[3];
  score += (int)model::bb::popcount(wQ) * base_value[4];

  score -= (int)model::bb::popcount(bP) * base_value[0];
  score -= (int)model::bb::popcount(bN) * base_value[1];
  score -= (int)model::bb::popcount(bB) * base_value[2];
  score -= (int)model::bb::popcount(bR) * base_value[3];
  score -= (int)model::bb::popcount(bQ) * base_value[4];

  // PST/center/development per piece
  auto do_side = [&](uint64_t pieces, int pieceType, int sign /* +1 white, -1 black */) {
    uint64_t bb = pieces;
    while (bb) {
      uint64_t lsb = bb & -bb;
      int sq = model::bb::ctz64(bb);
      bb -= lsb;
      int bonus = piece_square_bonus(pieceType, sq, sign > 0, isEndgame);
      score += sign * bonus;
      // back-rank penalty for minor pieces (not developed)
      if ((pieceType == 1 || pieceType == 2) && ((sq / 8) == (sign > 0 ? 0 : 7))) {
        score -= sign * BACKRANK_MINOR_PENALTY;
      }
    }
  };

  do_side(wN, 1, +1);
  do_side(wB, 2, +1);
  do_side(wR, 3, +1);
  do_side(wQ, 4, +1);
  do_side(bN, 1, -1);
  do_side(bB, 2, -1);
  do_side(bR, 3, -1);
  do_side(bQ, 4, -1);

  // bishop pair bonus
  if (model::bb::popcount(wB) >= 2) score += BISHOP_PAIR_BONUS;
  if (model::bb::popcount(bB) >= 2) score -= BISHOP_PAIR_BONUS;

  // Pawn structure: isolated, doubled, passed
  // Precompute files pawns
  for (int f = 0; f < 8; ++f) {
    uint64_t mask = file_mask(f);
    int wcount = model::bb::popcount(wP & mask);
    int bcount = model::bb::popcount(bP & mask);
    if (wcount > 1) score -= DOUBLED_PAWN_PENALTY * (wcount - 1);
    if (bcount > 1) score += DOUBLED_PAWN_PENALTY * (bcount - 1);
  }

  // isolated pawns and passed pawns
  auto is_isolated = [&](uint64_t pawns, int sq) -> bool {
    int f = sq % 8;
    uint64_t adj = 0;
    if (f > 0) adj |= file_mask(f - 1);
    if (f < 7) adj |= file_mask(f + 1);
    return (pawns & adj) == 0;
  };

  // passed pawn mask generators (approx)
  auto white_passed_mask = [&](int sq) -> uint64_t {
    int f = sq % 8;
    int r = sq / 8;
    uint64_t mask = 0;
    for (int rr = r + 1; rr < 8; ++rr) {
      int base = rr * 8;
      for (int ff = std::max(0, f - 1); ff <= std::min(7, f + 1); ++ff) {
        mask |= (1ULL << (base + ff));
      }
    }
    return mask;
  };
  auto black_passed_mask = [&](int sq) -> uint64_t {
    int f = sq % 8;
    int r = sq / 8;
    uint64_t mask = 0;
    for (int rr = r - 1; rr >= 0; --rr) {
      int base = rr * 8;
      for (int ff = std::max(0, f - 1); ff <= std::min(7, f + 1); ++ff) {
        mask |= (1ULL << (base + ff));
      }
    }
    return mask;
  };

  // iterate white pawns
  {
    uint64_t wp = wP;
    while (wp) {
      uint64_t lsb = wp & -wp;
      int sq = model::bb::ctz64(wp);
      wp -= lsb;
      // isolated
      if (is_isolated(wP, sq)) score -= ISOLATED_PAWN_PENALTY;
      // passed?
      uint64_t mask = white_passed_mask(sq);
      if ((bP & mask) == 0) {
        int r = sq / 8;
        score += PASSED_PAWN_BONUS_ADVANCE[r];
      }
    }
  }
  // iterate black pawns
  {
    uint64_t bp = bP;
    while (bp) {
      uint64_t lsb = bp & -bp;
      int sq = model::bb::ctz64(bp);
      bp -= lsb;
      if (is_isolated(bP, sq)) score += ISOLATED_PAWN_PENALTY;
      uint64_t mask = black_passed_mask(sq);
      if ((wP & mask) == 0) {
        int r = sq / 8;
        // for black, rank 6 is starting advanced for black, reward using mirrored index
        score -= PASSED_PAWN_BONUS_ADVANCE[7 - r];
      }
    }
  }

  // Rook open-file bonus (cheap: check if no pawns on that file)
  auto add_rook_open_bonus = [&](uint64_t rooks, uint64_t ownP, uint64_t oppP, int sign) {
    uint64_t rr = rooks;
    while (rr) {
      uint64_t lsb = rr & -rr;
      int sq = model::bb::ctz64(rr);
      rr -= lsb;
      int f = sq % 8;
      uint64_t mask = file_mask(f);
      if ((ownP & mask) == 0 && (oppP & mask) == 0) {
        score += sign * ROOK_OPEN_FILE_BONUS;
      } else if ((ownP & mask) == 0) {
        // semi-open
        score += sign * (ROOK_OPEN_FILE_BONUS / 2);
      }
    }
  };
  add_rook_open_bonus(wR, wP, bP, +1);
  add_rook_open_bonus(bR, bP, wP, -1);

  // King pawn shield: check pawns in front of king (simple)
  auto king_shield_penalty = [&](uint64_t king_bb, uint64_t ownP, int sign) {
    if (!king_bb) return;
    int ksq = model::bb::ctz64(king_bb);
    int kr = ksq / 8;
    int kf = ksq % 8;
    int shield_missing = 0;
    // squares in front depend on color sign
    if (sign > 0) {  // white king: check squares on ranks kr+1 and kr+2 in same/adj files
      for (int rr = kr + 1; rr <= std::min(7, kr + 2); ++rr) {
        for (int ff = std::max(0, kf - 1); ff <= std::min(7, kf + 1); ++ff) {
          if ((ownP & (1ULL << (rr * 8 + ff))) == 0) shield_missing++;
        }
      }
      score -= sign * (shield_missing * KING_PAWN_SHIELD_MISSING_PENALTY / 6);
    } else {  // black king: check downwards
      for (int rr = std::max(0, kr - 2); rr <= kr - 1; ++rr) {
        for (int ff = std::max(0, kf - 1); ff <= std::min(7, kf + 1); ++ff) {
          if ((ownP & (1ULL << (rr * 8 + ff))) == 0) shield_missing++;
        }
      }
      score -= sign * (shield_missing * KING_PAWN_SHIELD_MISSING_PENALTY / 6);
    }
  };

  king_shield_penalty(wK, wP, +1);
  king_shield_penalty(bK, bP, -1);

  return score;
}

}  // namespace lilia
