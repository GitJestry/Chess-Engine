#include "lilia/model/zobrist.hpp"

#include <atomic>
#include <mutex>

namespace lilia::model {

// Static storage
bb::Bitboard Zobrist::piece[2][6][64];
bb::Bitboard Zobrist::castling[16];
bb::Bitboard Zobrist::epFile[8];
bb::Bitboard Zobrist::side;

namespace {
// SplitMix64: schnell, gute Streuung, deterministisch
static inline std::uint64_t splitmix64(std::uint64_t& x) {
  std::uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

// Thread-sichere Einmal-Init für Zobrist::init()
std::once_flag g_once_init;
}  // namespace

void Zobrist::init(std::uint64_t seed) {
  // Fülle alle Keys; vermeide 0-Werte (sehr selten, aber sicher ist sicher)
  auto next = [&]() {
    std::uint64_t v;
    do {
      v = splitmix64(seed);
    } while (v == 0);
    return v;
  };

  for (int c = 0; c < 2; ++c) {
    for (int t = 0; t < 6; ++t) {
      for (int s = 0; s < 64; ++s) {
        piece[c][t][s] = next();
      }
    }
  }

  for (int i = 0; i < 16; ++i) castling[i] = next();
  for (int f = 0; f < 8; ++f) epFile[f] = next();

  side = next();
}

void Zobrist::init() {
  // Fester, reproduzierbarer Seed
  std::call_once(g_once_init, [] { Zobrist::init(0xC0FFEE123456789ULL); });
}

// EP nur dann hashen, wenn ein (pseudo-legaler) EP-Schlag existiert.
// Das spart viele unnötige TT-Kollisionen und fixt Repetition-Bugs.
bb::Bitboard Zobrist::epHashIfRelevant(const Board& b, const GameState& st) {
  if (st.enPassantSquare == core::NO_SQUARE) return 0;

  const core::Square ep = st.enPassantSquare;
  const int file = static_cast<int>(ep) & 7;  // 0..7

  // Gibt es eine Seite-zu-Zug Bauernfigur, die das EP-Feld angreifen kann?
  const auto sideToMove = st.sideToMove;
  bb::Bitboard pawns = b.getPieces(sideToMove, core::PieceType::Pawn);
  if (!pawns) return 0;

  const bb::Bitboard epBB = bb::sq_bb(ep);
  const bb::Bitboard attacks = (sideToMove == core::Color::White) ? bb::white_pawn_attacks(pawns)
                                                                  : bb::black_pawn_attacks(pawns);

  // Pseudo-legal reicht (kein voller Legalitätscheck nötig)
  if (attacks & epBB) return epFile[file];
  return 0;
}

}  // namespace lilia::model
