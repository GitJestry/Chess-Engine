#pragma once
#include <array>
#include <cstdint>

#include "lilia/model/core/bitboard.hpp"

namespace lilia::engine {
using namespace lilia::core;
using namespace lilia::model;

// -------- helpers (inline) --------
inline constexpr int mirror_sq_black(int sq) noexcept {
  return sq ^ 56;
}

// =============================================================================
// Globale Skalen & Mischer
// =============================================================================
constexpr int MAX_PHASE = 16;
inline int taper(int mg, int eg, int phase) {
  // mg when phase=MAX_PHASE, eg when phase=0
  return ((mg * phase) + (eg * (MAX_PHASE - phase))) / MAX_PHASE;
}

// Tempo (etwas moderater im EG)
constexpr int TEMPO_MG = 12;
constexpr int TEMPO_EG = 6;

// Space-Term im EG abgeschwächt
constexpr int SPACE_EG_DEN = 3;

// --- Pins
inline constexpr int PIN_MINOR = 14;
inline constexpr int PIN_ROOK = 10;
inline constexpr int PIN_QUEEN = 6;

// --- Safe checks (MG-heavy)
inline constexpr int KS_SAFE_CHECK_N = 12;
inline constexpr int KS_SAFE_CHECK_B = 10;
inline constexpr int KS_SAFE_CHECK_R = 14;
inline constexpr int KS_SAFE_CHECK_QB = 8;   // queen on bishop line
inline constexpr int KS_SAFE_CHECK_QR = 10;  // queen on rook line

// --- Holes
inline constexpr int HOLE_OCC_KN = 8;  // knight sitting on enemy-half hole
inline constexpr int HOLE_ATT_BI = 3;  // bishop attacks hole in king ring (per square)

// --- Pawn levers
inline constexpr int PAWN_LEVER_CENTER = 6;
inline constexpr int PAWN_LEVER_WING = 3;

// --- X-ray king-file pressure
inline constexpr int XRAY_KFILE = 4;

// --- Q+B battery toward king
inline constexpr int QB_BATTERY = 6;

// --- Central blockers (opening-weighted)
inline constexpr int CENTER_BLOCK_PEN = 6;
// after: tie them to MAX_PHASE so “opening” really means early-phase
constexpr int CENTER_BLOCK_PHASE_MAX = MAX_PHASE;
constexpr int CENTER_BLOCK_PHASE_DEN = MAX_PHASE;

// --- Weakly-defended (soft pressure)
inline constexpr int WEAK_MINOR = 6;
inline constexpr int WEAK_ROOK = 8;
inline constexpr int WEAK_QUEEN = 12;

// --- Fianchetto structure near king (MG)
inline constexpr int FIANCHETTO_OK = 6;
inline constexpr int FIANCHETTO_HOLE = 8;

// =============================================================================
// Pawns
// =============================================================================
constexpr int ISO_P = 12;
constexpr int DOUBLED_P = 16;
constexpr int BACKWARD_P = 8;
constexpr int PHALANX = 8;
constexpr int CANDIDATE_P = 10;
constexpr int CONNECTED_PASSERS = 20;

constexpr int PASSED_MG[8] = {0, 4, 8, 16, 36, 78, 150, 0};
constexpr int PASSED_EG[8] = {0, 8, 14, 28, 64, 132, 230, 0};

// Zusatzbedingungen
constexpr int PASS_BLOCK = 12;      // Blockade vor dem Passer
constexpr int PASS_FREE = 16;       // freie Vorzugsbahn
constexpr int PASS_KBOOST = 16;     // eigener König nahe
constexpr int PASS_KBLOCK = 16;     // gegnerischer König blockt
constexpr int PASS_PIECE_SUPP = 8;  // gedeckt durch Figur
constexpr int PASS_KPROX = 4;       // gegnerischer König in Nähe (Abzug)

// =============================================================================
// King safety (Druckgewichtung & Clamp)
// =============================================================================
constexpr int KS_W_N = 16, KS_W_B = 18, KS_W_R = 12, KS_W_Q = 24;
constexpr int KS_RING_BONUS = 1;
constexpr int KS_MISS_SHIELD = 8;
constexpr int KS_OPEN_FILE = 10;
constexpr int KS_CLAMP = 224;

// Geometrie / Power-Counting
constexpr int KING_RING_RADIUS = 2;
constexpr int KING_SHIELD_DEPTH = 2;
constexpr int KS_POWER_COUNT_CLAMP = 12;

// Mischung MG/EG (mit/ohne Damen; HeavyPieces = R+Q beider Seiten)
constexpr int KS_MIX_MG_Q_ON = 100;
constexpr int KS_MIX_MG_Q_OFF = 55;
constexpr int KS_MIX_EG_HEAVY_THRESHOLD = 2;
constexpr int KS_MIX_EG_IF_HEAVY = 40;
constexpr int KS_MIX_EG_IF_LIGHT = 18;

// =============================================================================
// King pawn shelter / storm
// =============================================================================
static constexpr int SHELTER[8] = {0, 0, 2, 6, 12, 20, 28, 34};
static constexpr int STORM[8] = {0, 6, 9, 12, 16, 20, 24, 28};
constexpr int SHELTER_EG_DEN = 4;

// =============================================================================
// Pieces/style
// =============================================================================
constexpr int BISHOP_PAIR = 32;

constexpr int BAD_BISHOP_PER_PAWN = 2;
constexpr int BAD_BISHOP_SAME_COLOR_THRESHOLD = 4;
constexpr int BAD_BISHOP_OPEN_NUM = 1;
constexpr int BAD_BISHOP_OPEN_DEN = 2;

constexpr int OUTPOST_KN = 24;
constexpr int OUTPOST_DEEP_RANK_WHITE = 4;  // r >= 4
constexpr int OUTPOST_DEEP_RANK_BLACK = 3;  // r <= 3
constexpr int OUTPOST_DEEP_EXTRA = 6;
constexpr int CENTER_CTRL = 6;
constexpr int OUTPOST_CENTER_SQ_BONUS = 6;

constexpr int KNIGHT_RIM = 12;

constexpr int ROOK_OPEN = 18;
constexpr int ROOK_SEMI = 10;
constexpr int ROOK_ON_7TH = 20;
constexpr int CONNECTED_ROOKS = 14;

constexpr int ROOK_BEHIND_PASSER = 24;
constexpr int ROOK_BEHIND_PASSER_HALF = ROOK_BEHIND_PASSER / 2;
constexpr int ROOK_BEHIND_PASSER_THIRD = ROOK_BEHIND_PASSER / 3;

// Rook vs Königsdatei (MG-Effekt)
constexpr int ROOK_SEMI_ON_KING_FILE = 6;
constexpr int ROOK_OPEN_ON_KING_FILE = 10;

// EG-only Rook-Extras
constexpr int ROOK_PASSER_PROGRESS_START_RANK = 3;
constexpr int ROOK_PASSER_PROGRESS_MULT = ROOK_BEHIND_PASSER_THIRD;
constexpr int ROOK_CUT_MIN_SEPARATION = 2;
constexpr int ROOK_CUT_BONUS = 12;

// Stopper-Qualität (wer blockiert das Stoppfeld?)
constexpr int BLOCK_PASSER_STOP_KNIGHT = 8;  // gut
constexpr int BLOCK_PASSER_STOP_BISHOP = 8;  // schlecht (als Malus ggü. gut)

// =============================================================================
// Threats & Hänger
// =============================================================================
constexpr int THR_PAWN_MINOR = 8;
constexpr int THR_PAWN_ROOK = 16;
constexpr int THR_PAWN_QUEEN = 20;

constexpr int HANG_MINOR = 10;
constexpr int HANG_ROOK = 14;
constexpr int HANG_QUEEN = 22;

constexpr int MINOR_ON_QUEEN = 6;

// Mischung: Threats stark im MG, deutlich geringer im EG
constexpr int THREATS_MG_NUM = 3, THREATS_MG_DEN = 2;  // *1.5
constexpr int THREATS_EG_DEN = 4;

// =============================================================================
// Space
// =============================================================================
constexpr int SPACE_BASE = 4;
constexpr int SPACE_SCALE_BASE = 2;  // 2 + min(#Minors, Sättigung)
constexpr int SPACE_MINOR_SATURATION = 4;

// =============================================================================
// Entwicklung & Blockaden
// =============================================================================
constexpr int DEVELOPMENT_PIECE_ON_HOME_PENALTY = 12;
constexpr int DEVELOPMENT_ROOK_ON_HOME_PENALTY = 8;
constexpr int DEVELOPMENT_QUEEN_ON_HOME_PENALTY = 10;
constexpr int DEV_MG_PHASE_CUTOFF = 12;
constexpr int DEV_MG_PHASE_DEN = 12;
constexpr int DEV_EG_DEN = 8;

constexpr int PIECE_BLOCKING_PENALTY = 8;

// =============================================================================
// King-Tropism
// =============================================================================
constexpr int TROPISM_BASE_KN = 12;
constexpr int TROPISM_BASE_BI = 10;
constexpr int TROPISM_BASE_RO = 8;
constexpr int TROPISM_BASE_QU = 6;
constexpr int TROPISM_DIST_FACTOR = 2;  // base - 2*Dist
constexpr int TROPISM_EG_DEN = 2;

// King Aktivität EG
constexpr int KING_ACTIVITY_EG_MULT = 2;

// =============================================================================
// Passed-pawn-race (EG, figurenarm)
// =============================================================================
constexpr int PASS_RACE_MAX_MINORMAJOR = 2;      // max. N/B/R total (ohne Damen)
constexpr bool PASS_RACE_NEED_QUEENLESS = true;  // nur ohne Damen
constexpr int PASS_RACE_STM_ADJ = 1;             // side-to-move Vorteil
constexpr int PASS_RACE_MULT = 4;

// =============================================================================
// Endgame scaling
// =============================================================================
constexpr int FULL_SCALE = 256;
constexpr int SCALE_DRAW = 0;
constexpr int SCALE_VERY_DRAWISH = 96;  // ~0.375
constexpr int SCALE_REDUCED = 144;      // ~0.56
constexpr int SCALE_MEDIUM = 160;       // ~0.625
constexpr int KN_CORNER_PAWN_SCALE = 32;
constexpr int OPP_BISHOPS_SCALE = 190;  // /256

// =============================================================================
// Castles & Center
// =============================================================================
inline bool rook_on_start_square(bb::Bitboard rooks, bool white) {
  return white ? (rooks & (bb::sq_bb(Square(0)) | bb::sq_bb(Square(7))))     // a1,h1
               : (rooks & (bb::sq_bb(Square(56)) | bb::sq_bb(Square(63))));  // a8,h8
}

constexpr int CASTLE_BONUS = 24;

constexpr int CENTER_BACK_PENALTY_Q_ON = 32;   // König im Zentrum (e/d) mit Damen
constexpr int CENTER_BACK_PENALTY_Q_OFF = 12;  // ohne Damen schwächer
constexpr int CENTER_BACK_OPEN_FILE_OPEN = 2;  // offene/halb-offene d/e-Dateien verstärken
constexpr int CENTER_BACK_OPEN_FILE_SEMI = 1;
constexpr int CENTER_BACK_OPEN_FILE_WEIGHT = 6;

constexpr int ROOK_KFILE_PRESS_FREE = 2;     // pro freiem Feld in der Linie zum K
constexpr int ROOK_KFILE_PRESS_PAWNATT = 3;  // Abzug wenn Feld von Bauern gedeckt
constexpr int ROOK_LIFT_SAFE = 6;

constexpr int KS_ESCAPE_EMPTY = 6;  // Basis für Fluchtfelder
constexpr int KS_ESCAPE_FACTOR = 2;

constexpr int EARLY_QUEEN_MALUS = 8;  // Frühe Dame bei Minors auf Grundreihe
constexpr int UNCASTLED_PENALTY_Q_ON = 10;

// =============================================================================
// Mobility Profile & Clamp
// =============================================================================
static constexpr int KN_MOB_MG[9] = {-14, -8, -4, 0, 4, 8, 12, 16, 18};
static constexpr int KN_MOB_EG[9] = {-10, -6, -2, 2, 6, 10, 12, 14, 16};

static constexpr int BI_MOB_MG[14] = {-18, -12, -6, -2, 2, 6, 10, 14, 18, 22, 24, 26, 28, 30};
static constexpr int BI_MOB_EG[14] = {-14, -10, -4, 0, 4, 8, 12, 16, 20, 24, 26, 28, 30, 32};

static constexpr int RO_MOB_MG[15] = {-18, -12, -6, -2, 2, 6, 10, 14, 18, 22, 26, 30, 32, 34, 36};
static constexpr int RO_MOB_EG[15] = {-10, -6, -2, 2, 6, 10, 14, 18, 22, 26, 30, 34, 36, 38, 40};

static constexpr int QU_MOB_MG[28] = {-8, -6, -4, -2, 0,  2,  4,  6,  8,  10, 12, 14, 16, 18,
                                      20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46};
static constexpr int QU_MOB_EG[28] = {-6, -4, -2, 0,  2,  4,  6,  8,  10, 12, 14, 16, 18, 20,
                                      22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48};

// Mobility-Output clamped (etwas enger als zuvor)
constexpr int MOBILITY_CLAMP = 512;

// =============================================================================
// Werte & Phase (white POV) — leicht SF-angelehnt
// =============================================================================
inline constexpr std::array<int, 6> VAL_MG = {82, 337, 365, 477, 1025, 0};
inline constexpr std::array<int, 6> VAL_EG = {94, 300, 320, 500, 940, 0};
inline constexpr std::array<int, 6> PHASE_W = {0, 1, 1, 2, 4, 0};

// =============================================================================
// PSTs (mg/eg) — belassen (engine-spezifisch)
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
