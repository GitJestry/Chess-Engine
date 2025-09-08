#pragma once
#include <array>
#include <cstdint>

#include "lilia/model/core/bitboard.hpp"

namespace lilia::engine {
using namespace lilia::core;
using namespace lilia::model;

// -------- helpers (inline) --------
inline int mirror_sq_black(int sq) noexcept {
  return sq ^ 56;
}

// =============================================================================
// Globale Skalen & Mischer
// =============================================================================
inline int MAX_PHASE = 24;     // sum both sides
inline int BLEND_SCALE = 256;  // MG/EG-Blendskala

inline int PAWN_VALUE = 100;
inline int KNIGHT_VALUE = 320;
inline int BISHOP_VALUE = 330;
inline int ROOK_VALUE = 500;
inline int QUEEN_VALUE = 900;

// Tempo
inline int TEMPO_MG = 20;
inline int TEMPO_EG = 8;
inline int SPACE_EG_DEN = 4;

// =============================================================================
// Pawns (leicht strenger auf Schwächen; stärkere Passer-Skalierung)
// =============================================================================
inline int ISO_P = 12;
inline int DOUBLED_P = 18;
inline int BACKWARD_P = 10;
inline int PHALANX = 8;
inline int CANDIDATE_P = 10;
inline int CONNECTED_PASSERS = 22;
// Passed pawn bonuses by rank. Heavily boosted for advanced passers
// so that near-promotion threats can outweigh small material deficits.
// Ranks 5-7 receive extra emphasis compared to the previous tuning.
inline int PASSED_MG[8] = {0, 8, 16, 30, 70, 190, 300, 0};
inline int PASSED_EG[8] = {0, 16, 28, 50, 110, 220, 340, 0};

// Additional bonuses/penalties for passer conditions
inline int PASS_BLOCK = 16;   // penalty if path is blocked
inline int PASS_SUPP = 16;    // own pawn support
inline int PASS_FREE = 24;    // no piece ahead
inline int PASS_KBOOST = 20;  // friendly king nearby
inline int PASS_KBLOCK = 18;  // enemy king in front
inline int PASS_PIECE_SUPP = 12;  // defended by own piece
inline int PASS_KPROX = 6;        // enemy king proximity penalty per step

// =============================================================================
/* King safety (SF-ähnliche Druckgewichtung & Clamp) */
// =============================================================================
inline int KS_W_N = 18, KS_W_B = 20, KS_W_R = 16, KS_W_Q = 24;
inline int KS_RING_BONUS = 1, KS_MISS_SHIELD = 8, KS_OPEN_FILE = 10, KS_RQ_LOS = 6,
              KS_CLAMP = 160;

// King-Ring/Shield-Geometrie
inline int KING_RING_RADIUS = 2;       // Ring-Radius (Chebyshev)
inline int KING_SHIELD_DEPTH = 2;      // Bauernreihen vor dem König, die als „Shield“ zählen
inline int KS_POWER_COUNT_CLAMP = 12;  // max Angriffs-Feldanzahl für Power-Akkumulation

// King-safety Mischung (MG/EG)
inline int KS_MIX_MG_Q_ON = 100;
inline int KS_MIX_MG_Q_OFF = 45;
inline int KS_MIX_EG_HEAVY_THRESHOLD = 2;  // # (Q+R) beider Seiten
inline int KS_MIX_EG_IF_HEAVY = 40;
inline int KS_MIX_EG_IF_LIGHT = 10;

// =============================================================================
// King pawn shelter / storm (leicht steiler)
// =============================================================================
static inline int SHELTER[8] = {0, 0, 2, 6, 12, 20, 28, 34};
static inline int STORM[8] = {0, 6, 10, 14, 18, 22, 26, 30};
inline int SHELTER_EG_DEN = 4;  // Shelter schwächer im EG

// =============================================================================
// Pieces/style (klassischer angehaucht)
// =============================================================================
inline int BISHOP_PAIR = 36;
inline int BAD_BISHOP_PER_PAWN = 2;
inline int BAD_BISHOP_SAME_COLOR_THRESHOLD = 4;  // ab so vielen gleichfarbigen Bauern
inline int BAD_BISHOP_OPEN_NUM = 1;              // Faktor „offenes Zentrum“
inline int BAD_BISHOP_OPEN_DEN = 2;

inline int OUTPOST_KN = 28;
inline int OUTPOST_DEEP_RANK_WHITE = 4;  // r >= 4
inline int OUTPOST_DEEP_RANK_BLACK = 3;  // r <= 3
inline int OUTPOST_DEEP_EXTRA = 6;       // Zusatz zu OUTPOST_KN bei „deep“
inline int CENTER_CTRL = 6;              // Angriffssteuerung auf Zentrum
inline int OUTPOST_CENTER_SQ_BONUS = 6;  // Outpost steht direkt auf d4/e4/d5/e5

inline int KNIGHT_RIM = 14;

inline int ROOK_OPEN = 20;
inline int ROOK_SEMI = 12;
inline int ROOK_ON_7TH = 26;
inline int CONNECTED_ROOKS = 18;
inline int ROOK_BEHIND_PASSER = 26;
inline int ROOK_BEHIND_PASSER_HALF = ROOK_BEHIND_PASSER / 2;
inline int ROOK_BEHIND_PASSER_THIRD = ROOK_BEHIND_PASSER / 3;

// Zusatzeffekte Rook vs Königsdatei (MG-Effekt)
inline int ROOK_SEMI_ON_KING_FILE = 6;
inline int ROOK_OPEN_ON_KING_FILE = 10;

// EG-only Rook-Extras
inline int ROOK_PASSER_PROGRESS_START_RANK = 3;  // ab dieser „Fortschritt“-Schwelle
inline int ROOK_PASSER_PROGRESS_MULT = ROOK_BEHIND_PASSER_THIRD;
inline int ROOK_CUT_MIN_SEPARATION = 2;  // Mindestens 2 Linien Abstand = „cut“
inline int ROOK_CUT_BONUS = 14;

// Stopper-Qualität (Wer blockiert Passer-Stopfeld?)
inline int BLOCK_PASSER_STOP_KNIGHT = 8;  // gut
inline int BLOCK_PASSER_STOP_BISHOP = 8;  // schlecht (als Malus gegenüber gut)

// Threats (etwas härter auf Hänger / Bauern-Drohungen)
inline int THR_PAWN_MINOR = 10, THR_PAWN_ROOK = 20, THR_PAWN_QUEEN = 24;
inline int HANG_MINOR = 10, HANG_ROOK = 16, HANG_QUEEN = 24;
inline int MINOR_ON_QUEEN = 6;

// Threats/Tropism Mischung
inline int THREATS_MG_NUM = 3, THREATS_MG_DEN = 2;  // *3/2
inline int THREATS_EG_DEN = 4;

// Space (kleiner Stupser)
inline int SPACE_BASE = 5;
inline int SPACE_SCALE_BASE = 2;        // 2 + min(#Minors, Sättigung)
inline int SPACE_MINOR_SATURATION = 4;  // Sättigungsgrenze

// =============================================================================
// Entwicklung & Blockaden
// =============================================================================
inline int DEVELOPMENT_PIECE_ON_HOME_PENALTY = 16;
inline int DEV_MG_PHASE_CUTOFF = 12;
inline int DEV_MG_PHASE_DEN = 12;
inline int DEV_EG_DEN = 8;

inline int PIECE_BLOCKING_PENALTY = 8;

// =============================================================================
// King-Tropism
// =============================================================================
inline int TROPISM_BASE_KN = 12;
inline int TROPISM_BASE_BI = 10;
inline int TROPISM_BASE_RO = 8;
inline int TROPISM_BASE_QU = 6;
inline int TROPISM_DIST_FACTOR = 2;  // base - 2*Dist
inline int TROPISM_EG_DEN = 2;

// King Aktivität EG
inline int KING_ACTIVITY_EG_MULT = 2;

// =============================================================================
// Passed-pawn-race (EG, figurenarm)
// =============================================================================
inline int PASS_RACE_MAX_MINORMAJOR = 2;      // max. Leicht-/Schwerfiguren total (ohne Damen)
constexpr bool PASS_RACE_NEED_QUEENLESS = true;  // nur ohne Damen werten
inline int PASS_RACE_STM_ADJ = 1;             // Zugrecht-Offset (0 oder 1)
inline int PASS_RACE_MULT = 4;

// =============================================================================
// Endgame scaling (inkl. klassischer Sonderfälle)
// =============================================================================
inline int FULL_SCALE = 256;
inline int SCALE_DRAW = 0;
inline int SCALE_VERY_DRAWISH = 96;  // ~0.375
inline int SCALE_REDUCED = 144;      // ~0.56
inline int SCALE_MEDIUM = 160;       // ~0.625
inline int KN_CORNER_PAWN_SCALE = 32;

// Opposites (bisschen remislastiger) – wird explizit genutzt
inline int OPP_BISHOPS_SCALE = 190;  // /256

// =============================================================================
// Castles & Center
// =============================================================================
inline bool rook_on_start_square(bb::Bitboard rooks, bool white) {
  return white ? (rooks & (bb::sq_bb(Square(0)) | bb::sq_bb(Square(7))))     // a1,h1
               : (rooks & (bb::sq_bb(Square(56)) | bb::sq_bb(Square(63))));  // a8,h8
}

inline int CASTLE_BONUS = 28;

inline int CENTER_BACK_PENALTY_Q_ON = 36;   // König im Zentrum (e/d) bei Damen auf Brett
inline int CENTER_BACK_PENALTY_Q_OFF = 12;  // dito ohne Damen
inline int CENTER_BACK_OPEN_FILE_OPEN = 2;  // Bewertung offener/halb-offener d/e-File...
inline int CENTER_BACK_OPEN_FILE_SEMI = 1;  // ...als Verstärker
inline int CENTER_BACK_OPEN_FILE_WEIGHT = 8;
inline int ROOK_KFILE_PRESS_FREE = 2;     // bonus per un-attacked lane square
inline int ROOK_KFILE_PRESS_PAWNATT = 3;  // penalty per lane square attacked by enemy pawn
inline int ROOK_LIFT_SAFE = 6;
inline int KS_ESCAPE_EMPTY = 6, KS_ESCAPE_FACTOR = 2;
inline int EARLY_QUEEN_MALUS = 8;  // Frühe Dame vor unentwickelten Minors

inline int UNCASTLED_PENALTY_Q_ON = 10;  // „nicht rochiert“ Heuristik bei Q on

// =============================================================================
// Mobility Profile & Clamp
// =============================================================================
static inline int KN_MOB_MG[9] = {-16, -8, -4, 0, 4, 8, 12, 16, 18};
static inline int KN_MOB_EG[9] = {-12, -6, -2, 2, 6, 10, 12, 14, 16};
static inline int BI_MOB_MG[14] = {-22, -12, -6, -2, 2, 6, 10, 14, 18, 22, 24, 26, 28, 30};
static inline int BI_MOB_EG[14] = {-18, -10, -4, 0, 4, 8, 12, 16, 20, 24, 26, 28, 30, 32};
static inline int RO_MOB_MG[15] = {-20, -12, -6, -2, 2, 6, 10, 14, 18, 22, 26, 30, 32, 34, 36};
static inline int RO_MOB_EG[15] = {-10, -6, -2, 2, 6, 10, 14, 18, 22, 26, 30, 34, 36, 38, 40};
static inline int QU_MOB_MG[28] = {-10, -8, -6, -4, -2, 0,  2,  4,  6,  8,  10, 12, 14, 16,
                                      18,  20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44};
static inline int QU_MOB_EG[28] = {-6, -4, -2, 0,  2,  4,  6,  8,  10, 12, 14, 16, 18, 20,
                                      22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48};
inline int MOBILITY_CLAMP = 900;

// =============================================================================
// Werte & Phase (white POV)
// =============================================================================
inline constexpr std::array<int, 6> VAL_MG = {82, 337, 365, 477, 1025, 0};
inline constexpr std::array<int, 6> VAL_EG = {94, 300, 320, 500, 940, 0};
inline constexpr std::array<int, 6> PHASE_W = {0, 1, 1, 2, 4, 0};

// =============================================================================
// PSTs (mg/eg) – unverändert
// =============================================================================
inline constexpr std::array<int, 64> PST_P_MG = {
    0,  0,  0,  0,  0,  0,  0,  0,  6,  6,  2,  -6, -6, 2,  6,  6,  4,  -2, -3, 2,  2,  -3,
    -2, 4,  6,  8,  12, 16, 16, 12, 8,  6,  8,  12, 18, 24, 24, 18, 12, 8,  12, 18, 24, 28,
    28, 24, 18, 12, 12, 12, 12, 12, 12, 12, 12, 12, 0,  0,  0,  0,  0,  0,  0,  0};
inline constexpr std::array<int, 64> PST_P_EG = {
    0,  0,  0,  0,  0,  0,  0,  0,  6,  8,  4,  -2, -2, 4,  8,  6,  6,  2,  2,  6,  6,  2,
    2,  6,  8,  12, 16, 20, 20, 16, 12, 8,  12, 18, 24, 30, 30, 24, 18, 12, 16, 24, 32, 40,
    40, 32, 24, 16, 10, 14, 18, 22, 22, 18, 14, 10, 0,  0,  0,  0,  0,  0,  0,  0};
inline constexpr std::array<int, 64> PST_N_MG = {
    -50, -38, -28, -22, -22, -28, -38, -50, -32, -16, -4,  2,   2,   -4,  -16, -32,
    -24, -2,  12,  18,  18,  12,  -2,  -24, -20, 4,   18,  26,  26,  18,  4,   -20,
    -20, 4,   18,  26,  26,  18,  4,   -20, -24, -2,  12,  18,  18,  12,  -2,  -24,
    -34, -16, -4,  0,   0,   -4,  -16, -34, -46, -36, -28, -24, -24, -28, -36, -46};
inline constexpr std::array<int, 64> PST_N_EG = {
    -36, -26, -18, -14, -14, -18, -26, -36, -26, -12, -2,  6,   6,   -2,  -12, -26,
    -18, -2,  10,  16,  16,  10,  -2,  -18, -14, 6,   16,  22,  22,  16,  6,   -14,
    -14, 6,   16,  22,  22,  16,  6,   -14, -18, -2,  10,  16,  16,  10,  -2,  -18,
    -26, -12, -2,  6,   6,   -2,  -12, -26, -36, -26, -18, -14, -14, -18, -26, -36};
inline constexpr std::array<int, 64> PST_B_MG = {
    -26, -14, -10, -8, -8, -10, -14, -26, -12, -4,  2,  6,  6,  2,  -4,  -12,
    -8,  4,   10,  14, 14, 10,  4,   -8,  -6,  8,   14, 20, 20, 14, 8,   -6,
    -6,  8,   14,  20, 20, 14,  8,   -6,  -8,  4,   10, 14, 14, 10, 4,   -8,
    -12, -4,  2,   6,  6,  2,   -4,  -12, -24, -12, -8, -6, -6, -8, -12, -24};
inline constexpr std::array<int, 64> PST_B_EG = {
    -18, -8, -4, -2, -2, -4, -8, -18, -8, 0,  8,  12, 12,  8,  0,  -8, -4, 8,  14, 20, 20, 14,
    8,   -4, -2, 12, 20, 26, 26, 20,  12, -2, -2, 12, 20,  26, 26, 20, 12, -2, -4, 8,  14, 20,
    20,  14, 8,  -4, -8, 0,  8,  12,  12, 8,  0,  -8, -16, -8, -4, -2, -2, -4, -8, -16};
inline constexpr std::array<int, 64> PST_R_MG = {
    0,  2,  3,  4,  4, 3, 2, 0, -2, 0,  2,  4,  4, 2, 0, -2, -3, -1, 0,  2,  2, 0,
    -1, -3, -4, -1, 1, 2, 2, 1, -1, -4, -4, -1, 1, 2, 2, 1,  -1, -4, -3, -1, 0, 2,
    2,  0,  -1, -3, 4, 6, 6, 8, 8,  6,  6,  4,  2, 4, 4, 6,  6,  4,  4,  2};
inline constexpr std::array<int, 64> PST_R_EG = {
    2, 4,  6,  8,  8, 6, 4, 2, 0, 2,  4,  6, 6, 4, 2, 0,  -1, 1,  2,  4, 4, 2,
    1, -1, -1, 1,  2, 4, 4, 2, 1, -1, -1, 1, 2, 4, 4, 2,  1,  -1, -1, 1, 2, 4,
    4, 2,  1,  -1, 3, 5, 7, 9, 9, 7,  5,  3, 4, 6, 8, 10, 10, 8,  6,  4};
inline constexpr std::array<int, 64> PST_Q_MG = {
    -24, -16, -12, -8, -8, -12, -16, -24, -16, -8,  -4,  -2, -2, -4,  -8,  -16,
    -12, -4,  2,   4,  4,  2,   -4,  -12, -8,  -2,  4,   6,  6,  4,   -2,  -8,
    -8,  -2,  4,   6,  6,  4,   -2,  -8,  -12, -4,  2,   4,  4,  2,   -4,  -12,
    -16, -8,  -4,  -2, -2, -4,  -8,  -16, -24, -16, -12, -8, -8, -12, -16, -24};
inline constexpr std::array<int, 64> PST_Q_EG = {
    -10, -6, -2, 0,  0,  -2, -6, -10, -6, -2, 2,  4,  4,   2,  -2, -6, -2, 2,  6,  8,  8, 6,
    2,   -2, 0,  4,  8,  12, 12, 8,   4,  0,  0,  4,  8,   12, 12, 8,  4,  0,  -2, 2,  6, 8,
    8,   6,  2,  -2, -6, -2, 2,  4,   4,  2,  -2, -6, -10, -6, -2, 0,  0,  -2, -6, -10};
inline constexpr std::array<int, 64> PST_K_MG = {
    -40, -48, -52, -56, -56, -52, -48, -40, -32, -40, -44, -50, -50, -44, -40, -32,
    -24, -32, -36, -44, -44, -36, -32, -24, -12, -20, -28, -36, -36, -28, -20, -12,
    0,   -8,  -18, -28, -28, -18, -8,  0,   10,  18,  4,   -10, -10, 4,   18,  10,
    20,  28,  18,  6,   6,   18,  28,  20,  28,  38,  28,  12,  12,  28,  38,  28};
inline constexpr std::array<int, 64> PST_K_EG = {
    -8, -4, -4, -2, -2, -4, -4, -8, -4, 2,  4,  6,  6,  4,  2,  -4, -4, 4,  10, 12, 12, 10,
    4,  -4, -2, 6,  12, 18, 18, 12, 6,  -2, -2, 6,  12, 18, 18, 12, 6,  -2, -4, 4,  10, 12,
    12, 10, 4,  -4, -4, 2,  4,  6,  6,  4,  2,  -4, -8, -4, -4, -2, -2, -4, -4, -8};

// -------- PST accessors (inline) --------
inline int pst_mg(PieceType pt, int sq) {
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
inline int pst_eg(PieceType pt, int sq) {
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

}  // namespace lilia::engine
