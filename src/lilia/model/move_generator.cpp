#include "lilia/model/move_generator.hpp"

#include <array>
#include <cstdint>

#include "lilia/model/core/magic.hpp"
#include "lilia/model/move_helper.hpp"

namespace lilia::model {

namespace {

using core::Color;
using core::PieceType;
using core::Square;

using PT = core::PieceType;

struct SideSets {
  bb::Bitboard pawns, knights, bishops, rooks, queens, king, all;
};

inline SideSets side_sets(const Board& b, Color c) noexcept {
  return SideSets{b.getPieces(c, PT::Pawn),
                  b.getPieces(c, PT::Knight),
                  b.getPieces(c, PT::Bishop),
                  b.getPieces(c, PT::Rook),
                  b.getPieces(c, PT::Queen),
                  b.getPieces(c, PT::King),
                  b.getPieces(c)};
}

// ---------------- Between-Tabelle ----------------
// Precompute squares_between(a,b) -> Bitboard der strikt dazwischen liegenden Felder.
// Beschleunigt Pins und Evasions spürbar.

inline bb::Bitboard compute_between_single(int ai, int bi) noexcept {
  if (ai == bi) return 0ULL;
  const int d = bi - ai;
  int step = 0;
  if (d % 9 == 0)
    step = (d > 0 ? 9 : -9);
  else if (d % 7 == 0)
    step = (d > 0 ? 7 : -7);
  else if (ai / 8 == bi / 8)
    step = (d > 0 ? 1 : -1);
  else if ((ai % 8) == (bi % 8))
    step = (d > 0 ? 8 : -8);
  else
    return 0ULL;

  bb::Bitboard mask = 0ULL;
  for (int cur = ai + step; cur != bi; cur += step) mask |= bb::sq_bb(static_cast<Square>(cur));
  return mask;
}

std::array<std::array<bb::Bitboard, 64>, 64> Between = [] {
  std::array<std::array<bb::Bitboard, 64>, 64> T{};
  for (int a = 0; a < 64; ++a)
    for (int b = 0; b < 64; ++b) T[a][b] = compute_between_single(a, b);
  return T;
}();

inline bb::Bitboard squares_between(Square a, Square b) noexcept {
  return Between[(int)a][(int)b];
}

// ---------------- Pin-Infos (sparse) ----------------

struct PinEntry {
  uint8_t sq;          // pinned piece square
  bb::Bitboard allow;  // allowed destinations (line to pinner incl. pinner)
};

struct PinInfo {
  bb::Bitboard pinned = 0ULL;
  PinEntry entries[8]{};
  uint8_t count = 0;

  inline bb::Bitboard allow_mask(Square s) const noexcept {
    if (!(pinned & bb::sq_bb(s))) return ~0ULL;
    for (uint8_t i = 0; i < count; ++i)
      if (entries[i].sq == (uint8_t)s) return entries[i].allow;
    return 0ULL;  // sollte praktisch nie passieren
  }

  inline void add(Square pinnedSq, bb::Bitboard allow) noexcept {
    pinned |= bb::sq_bb(pinnedSq);
    entries[count++] = PinEntry{(uint8_t)pinnedSq, allow};
  }
};

// Nur Pinner auf Königsstrahlen betrachten, keine doppelte Between-Berechnung
inline void compute_pins(const Board& b, Color us, const bb::Bitboard occ, PinInfo& out) noexcept {
  out.pinned = 0ULL;
  out.count = 0;
  const bb::Bitboard kbb = b.getPieces(us, PT::King);
  if (!kbb) return;
  const Square ksq = static_cast<Square>(bb::ctz64(kbb));

  // For pin detection we need to look past our own pieces so that enemy sliders
  // behind them become visible.  Hence, remove all of our pieces from the
  // occupancy when tracing rays from the king.  The full occupancy is still
  // used later when counting blockers between the king and a potential pinner.
  const bb::Bitboard occNoUs = occ & ~b.getPieces(us);

  const bb::Bitboard diagFromK =
      magic::sliding_attacks(magic::Slider::Bishop, ksq, occNoUs) &
      (b.getPieces(~us, PT::Bishop) | b.getPieces(~us, PT::Queen));
  const bb::Bitboard orthoFromK =
      magic::sliding_attacks(magic::Slider::Rook, ksq, occNoUs) &
      (b.getPieces(~us, PT::Rook) | b.getPieces(~us, PT::Queen));

  auto try_mark = [&](Square pinnerSq) noexcept {
    const bb::Bitboard between = squares_between(ksq, pinnerSq);
    if (!between) return;
    const bb::Bitboard blockers = between & occ;
    // exactly one bit?  (x != 0) && (x & (x-1)) == 0
    if (!blockers || (blockers & (blockers - 1))) return;
    const bb::Bitboard ours = blockers & b.getPieces(us);
    if (!ours) return;
    const Square pinnedSq = static_cast<Square>(bb::ctz64(ours));
    out.add(pinnedSq, between | bb::sq_bb(pinnerSq));
  };

  for (bb::Bitboard s = diagFromK; s;) try_mark(bb::pop_lsb(s));
  for (bb::Bitboard s = orthoFromK; s;) try_mark(bb::pop_lsb(s));
}

// --------- schneller EP-Legalitätscheck (unverändert, minimal gesäubert) ---------
inline bool ep_is_legal_fast(const Board& b, Color side, Square from, Square to) noexcept {
  const bb::Bitboard kbb = b.getPieces(side, PT::King);
  if (!kbb) return false;
  const Square ksq = static_cast<Square>(bb::ctz64(kbb));

  // Relevanz nur bei gleicher Reihe
  if (bb::rank_of(ksq) != bb::rank_of(from)) return true;

  const int to_i = (int)to;
  const int cap_i = (side == Color::White) ? (to_i - 8) : (to_i + 8);
  const Square capSq = static_cast<Square>(cap_i);

  bb::Bitboard occ = b.getAllPieces();
  occ &= ~bb::sq_bb(from);
  occ &= ~bb::sq_bb(capSq);
  occ |= bb::sq_bb(to);

  const bb::Bitboard sliders = b.getPieces(~side, PT::Rook) | b.getPieces(~side, PT::Queen);
  const bb::Bitboard rays = magic::sliding_attacks(magic::Slider::Rook, ksq, occ);
  return (rays & sliders) == 0ULL;
}

// ---------------- Template-Generatoren ----------------

template <core::Color Side, class Emit>
inline void genPawnMoves_T(const Board& board, const GameState& st, bb::Bitboard occ,
                           const SideSets& our, const SideSets& opp, Emit&& emit,
                           bb::Bitboard targetMask = ~0ULL) noexcept {
  const bb::Bitboard empty = ~occ;
  const bb::Bitboard enemyAll = opp.all;
  const bb::Bitboard enemyKing = opp.king;
  const bb::Bitboard them = enemyAll & ~enemyKing;

  bb::Bitboard pawns = our.pawns;
  constexpr bool W = (Side == core::Color::White);

  if constexpr (W) {
    bb::Bitboard one = bb::north(pawns) & empty;
    bb::Bitboard dbl = bb::north(one & bb::RANK_3) & empty;

    bb::Bitboard promoPush = (one & bb::RANK_8) & targetMask;
    bb::Bitboard quietPush = (one & ~bb::RANK_8) & targetMask;

    bb::Bitboard capL = ((bb::nw(pawns) & them) & ~bb::RANK_8) & targetMask;
    bb::Bitboard capR = ((bb::ne(pawns) & them) & ~bb::RANK_8) & targetMask;

    for (bb::Bitboard q = quietPush; q;) {
      core::Square to = bb::pop_lsb(q);
      emit(Move{static_cast<core::Square>(to - 8), to, core::PieceType::None, false, false,
                CastleSide::None});
    }
    for (bb::Bitboard d = (dbl & targetMask); d;) {
      core::Square to = bb::pop_lsb(d);
      emit(Move{static_cast<core::Square>(to - 16), to, core::PieceType::None, false, false,
                CastleSide::None});
    }
    for (bb::Bitboard c = capL; c;) {
      core::Square to = bb::pop_lsb(c);
      emit(Move{static_cast<core::Square>(to - 7), to, core::PieceType::None, true, false,
                CastleSide::None});
    }
    for (bb::Bitboard c = capR; c;) {
      core::Square to = bb::pop_lsb(c);
      emit(Move{static_cast<core::Square>(to - 9), to, core::PieceType::None, true, false,
                CastleSide::None});
    }

    // Promotions
    constexpr core::PieceType promoOrder[4] = {core::PieceType::Queen, core::PieceType::Rook,
                                               core::PieceType::Bishop, core::PieceType::Knight};
    for (bb::Bitboard pp = promoPush; pp;) {
      core::Square to = bb::pop_lsb(pp);
      core::Square from = static_cast<core::Square>(to - 8);
      for (int i = 0; i < 4; ++i)
        emit(Move{from, to, promoOrder[i], false, false, CastleSide::None});
    }
    bb::Bitboard capLP = ((bb::nw(pawns) & them) & bb::RANK_8) & targetMask;
    bb::Bitboard capRP = ((bb::ne(pawns) & them) & bb::RANK_8) & targetMask;
    for (bb::Bitboard c = capLP; c;) {
      core::Square to = bb::pop_lsb(c);
      core::Square from = static_cast<core::Square>(to - 7);
      for (int i = 0; i < 4; ++i)
        emit(Move{from, to, promoOrder[i], true, false, CastleSide::None});
    }
    for (bb::Bitboard c = capRP; c;) {
      core::Square to = bb::pop_lsb(c);
      core::Square from = static_cast<core::Square>(to - 9);
      for (int i = 0; i < 4; ++i)
        emit(Move{from, to, promoOrder[i], true, false, CastleSide::None});
    }

  } else {  // Black
    bb::Bitboard one = bb::south(pawns) & empty;
    bb::Bitboard dbl = bb::south(one & bb::RANK_6) & empty;

    bb::Bitboard promoPush = (one & bb::RANK_1) & targetMask;
    bb::Bitboard quietPush = (one & ~bb::RANK_1) & targetMask;

    bb::Bitboard capL = ((bb::se(pawns) & them) & ~bb::RANK_1) & targetMask;
    bb::Bitboard capR = ((bb::sw(pawns) & them) & ~bb::RANK_1) & targetMask;

    for (bb::Bitboard q = quietPush; q;) {
      core::Square to = bb::pop_lsb(q);
      emit(Move{static_cast<core::Square>(to + 8), to, core::PieceType::None, false, false,
                CastleSide::None});
    }
    for (bb::Bitboard d = (dbl & targetMask); d;) {
      core::Square to = bb::pop_lsb(d);
      emit(Move{static_cast<core::Square>(to + 16), to, core::PieceType::None, false, false,
                CastleSide::None});
    }
    for (bb::Bitboard c = capL; c;) {
      core::Square to = bb::pop_lsb(c);
      emit(Move{static_cast<core::Square>(to + 7), to, core::PieceType::None, true, false,
                CastleSide::None});
    }
    for (bb::Bitboard c = capR; c;) {
      core::Square to = bb::pop_lsb(c);
      emit(Move{static_cast<core::Square>(to + 9), to, core::PieceType::None, true, false,
                CastleSide::None});
    }

    constexpr core::PieceType promoOrder[4] = {core::PieceType::Queen, core::PieceType::Rook,
                                               core::PieceType::Bishop, core::PieceType::Knight};
    for (bb::Bitboard pp = promoPush; pp;) {
      core::Square to = bb::pop_lsb(pp);
      core::Square from = static_cast<core::Square>(to + 8);
      for (int i = 0; i < 4; ++i)
        emit(Move{from, to, promoOrder[i], false, false, CastleSide::None});
    }
    bb::Bitboard capLP = ((bb::se(pawns) & them) & bb::RANK_1) & targetMask;
    bb::Bitboard capRP = ((bb::sw(pawns) & them) & bb::RANK_1) & targetMask;
    for (bb::Bitboard c = capLP; c;) {
      core::Square to = bb::pop_lsb(c);
      core::Square from = static_cast<core::Square>(to + 7);
      for (int i = 0; i < 4; ++i)
        emit(Move{from, to, promoOrder[i], true, false, CastleSide::None});
    }
    for (bb::Bitboard c = capRP; c;) {
      core::Square to = bb::pop_lsb(c);
      core::Square from = static_cast<core::Square>(to + 9);
      for (int i = 0; i < 4; ++i)
        emit(Move{from, to, promoOrder[i], true, false, CastleSide::None});
    }
  }

  // En passant (bewusst NICHT über targetMask gefiltert; Korrektheit geht vor)
  if (st.enPassantSquare != core::NO_SQUARE) {
    const core::Square epSq = st.enPassantSquare;
    const bb::Bitboard ep = bb::sq_bb(epSq);
    if constexpr (W) {
      bb::Bitboard froms = (bb::sw(ep) | bb::se(ep)) & our.pawns;
      for (bb::Bitboard f = froms; f;) {
        core::Square from = bb::pop_lsb(f);
        if (ep_is_legal_fast(board, Side, from, epSq))
          emit(Move{from, epSq, core::PieceType::None, true, true, CastleSide::None});
      }
    } else {
      bb::Bitboard froms = (bb::nw(ep) | bb::ne(ep)) & our.pawns;
      for (bb::Bitboard f = froms; f;) {
        core::Square from = bb::pop_lsb(f);
        if (ep_is_legal_fast(board, Side, from, epSq))
          emit(Move{from, epSq, core::PieceType::None, true, true, CastleSide::None});
      }
    }
  }
}

template <class Emit>
inline void genKnightMoves_T(const Board& /*board*/, const SideSets& our, const SideSets& opp,
                             bb::Bitboard occ, Emit&& emit,
                             bb::Bitboard targetMask = ~0ULL) noexcept {
  const bb::Bitboard knights = our.knights;
  const bb::Bitboard enemyNoK = opp.all & ~opp.king;

  for (bb::Bitboard n = knights; n;) {
    core::Square from = bb::pop_lsb(n);
    bb::Bitboard atk = bb::knight_attacks_from(from) & targetMask;
    for (bb::Bitboard caps = atk & enemyNoK; caps;) {
      emit(Move{from, bb::pop_lsb(caps), core::PieceType::None, true, false, CastleSide::None});
    }
    for (bb::Bitboard quiet = atk & ~occ; quiet;) {
      emit(Move{from, bb::pop_lsb(quiet), core::PieceType::None, false, false, CastleSide::None});
    }
  }
}

template <class Emit>
inline void genBishopMoves_T(const SideSets& our, const SideSets& opp, bb::Bitboard occ,
                             Emit&& emit, bb::Bitboard targetMask = ~0ULL) noexcept {
  const bb::Bitboard bishops = our.bishops;
  const bb::Bitboard enemyNoK = opp.all & ~opp.king;

  for (bb::Bitboard b = bishops; b;) {
    core::Square from = bb::pop_lsb(b);
    bb::Bitboard atk = magic::sliding_attacks(magic::Slider::Bishop, from, occ) & targetMask;
    for (bb::Bitboard caps = atk & enemyNoK; caps;) {
      emit(Move{from, bb::pop_lsb(caps), core::PieceType::None, true, false, CastleSide::None});
    }
    for (bb::Bitboard quiet = atk & ~occ; quiet;) {
      emit(Move{from, bb::pop_lsb(quiet), core::PieceType::None, false, false, CastleSide::None});
    }
  }
}

template <class Emit>
inline void genRookMoves_T(const SideSets& our, const SideSets& opp, bb::Bitboard occ, Emit&& emit,
                           bb::Bitboard targetMask = ~0ULL) noexcept {
  const bb::Bitboard rooks = our.rooks;
  const bb::Bitboard enemyNoK = opp.all & ~opp.king;

  for (bb::Bitboard r = rooks; r;) {
    core::Square from = bb::pop_lsb(r);
    bb::Bitboard atk = magic::sliding_attacks(magic::Slider::Rook, from, occ) & targetMask;
    for (bb::Bitboard caps = atk & enemyNoK; caps;) {
      emit(Move{from, bb::pop_lsb(caps), core::PieceType::None, true, false, CastleSide::None});
    }
    for (bb::Bitboard quiet = atk & ~occ; quiet;) {
      emit(Move{from, bb::pop_lsb(quiet), core::PieceType::None, false, false, CastleSide::None});
    }
  }
}
template <class Emit>
inline void genQueenMoves_T(const SideSets& our, const SideSets& opp, bb::Bitboard occ, Emit&& emit,
                            bb::Bitboard targetMask = ~0ULL) noexcept {
  const bb::Bitboard queens = our.queens;
  const bb::Bitboard enemyNoK = opp.all & ~opp.king;

  for (bb::Bitboard q = queens; q;) {
    core::Square from = bb::pop_lsb(q);
    bb::Bitboard atk = (magic::sliding_attacks(magic::Slider::Bishop, from, occ) |
                        magic::sliding_attacks(magic::Slider::Rook, from, occ)) &
                       targetMask;
    for (bb::Bitboard caps = atk & enemyNoK; caps;) {
      emit(Move{from, bb::pop_lsb(caps), core::PieceType::None, true, false, CastleSide::None});
    }
    for (bb::Bitboard quiet = atk & ~occ; quiet;) {
      emit(Move{from, bb::pop_lsb(quiet), core::PieceType::None, false, false, CastleSide::None});
    }
  }
}

template <class Emit>
inline void genKingMoves_T(const Board& board, const GameState& st, Color side, const SideSets& our,
                           const SideSets& opp, bb::Bitboard occ, Emit&& emit) noexcept {
  const bb::Bitboard king = our.king;
  if (!king) return;
  const Square from = static_cast<Square>(bb::ctz64(king));

  const bb::Bitboard enemyK = opp.king;
  const bb::Bitboard enemyNoK = opp.all & ~enemyK;
  const bb::Bitboard atk = bb::king_attacks_from(from);

  const bb::Bitboard fromBB = bb::sq_bb(from);

  // Captures
  for (bb::Bitboard caps = atk & enemyNoK; caps;) {
    Square to = bb::pop_lsb(caps);
    const bb::Bitboard toBB = bb::sq_bb(to);
    const bb::Bitboard occ2 = (occ & ~fromBB & ~toBB);  // König weg, geschlagener weg
    if (!attackedBy(board, to, ~side, occ2))
      emit(Move{from, to, PT::None, true, false, CastleSide::None});
  }

  // Quiet
  for (bb::Bitboard quiet = atk & ~occ; quiet;) {
    Square to = bb::pop_lsb(quiet);
    const bb::Bitboard occ2 = (occ & ~fromBB);  // König weg
    if (!attackedBy(board, to, ~side, occ2))
      emit(Move{from, to, PT::None, false, false, CastleSide::None});
  }

  // Rochade
  const Color enemySide = ~side;
  if (side == Color::White) {
    if ((st.castlingRights & bb::Castling::WK) && (our.rooks & bb::sq_bb(bb::H1)) &&
        !(occ & (bb::sq_bb(Square{5}) | bb::sq_bb(Square{6})))) {
      if (!attackedBy(board, Square{4}, enemySide, occ) &&
          !attackedBy(board, Square{5}, enemySide, occ) &&
          !attackedBy(board, Square{6}, enemySide, occ)) {
        emit(Move{bb::E1, Square{6}, PT::None, false, false, CastleSide::KingSide});
      }
    }
    if ((st.castlingRights & bb::Castling::WQ) && (our.rooks & bb::sq_bb(bb::A1)) &&
        !(occ & (bb::sq_bb(Square{3}) | bb::sq_bb(Square{2}) | bb::sq_bb(Square{1})))) {
      if (!attackedBy(board, Square{4}, enemySide, occ) &&
          !attackedBy(board, Square{3}, enemySide, occ) &&
          !attackedBy(board, Square{2}, enemySide, occ)) {
        emit(Move{bb::E1, Square{2}, PT::None, false, false, CastleSide::QueenSide});
      }
    }
  } else {
    if ((st.castlingRights & bb::Castling::BK) && (our.rooks & bb::sq_bb(bb::H8)) &&
        !(occ & (bb::sq_bb(Square{61}) | bb::sq_bb(Square{62})))) {
      if (!attackedBy(board, Square{60}, enemySide, occ) &&
          !attackedBy(board, Square{61}, enemySide, occ) &&
          !attackedBy(board, Square{62}, enemySide, occ)) {
        emit(Move{bb::E8, Square{62}, PT::None, false, false, CastleSide::KingSide});
      }
    }
    if ((st.castlingRights & bb::Castling::BQ) && (our.rooks & bb::sq_bb(bb::A8)) &&
        !(occ & (bb::sq_bb(Square{59}) | bb::sq_bb(Square{58}) | bb::sq_bb(Square{57})))) {
      if (!attackedBy(board, Square{60}, enemySide, occ) &&
          !attackedBy(board, Square{59}, enemySide, occ) &&
          !attackedBy(board, Square{58}, enemySide, occ)) {
        emit(Move{bb::E8, Square{58}, PT::None, false, false, CastleSide::QueenSide});
      }
    }
  }
}

// ---------- Evasions-Generator (verbessert: Angreifer vom König aus) ----------
template <class Emit>
inline void generateEvasions_T(const Board& b, const GameState& st, Emit&& emit) noexcept {
  const core::Color us = st.sideToMove;
  const core::Color them = ~us;

  const bb::Bitboard kbb = b.getPieces(us, core::PieceType::King);
  if (!kbb) return;
  const core::Square ksq = static_cast<core::Square>(bb::ctz64(kbb));

  const bb::Bitboard occ = b.getAllPieces();

  // Checkers vom König aus bestimmen
  bb::Bitboard checkers = 0ULL;
  if (us == core::Color::White) {
    checkers |= (bb::sw(bb::sq_bb(ksq)) | bb::se(bb::sq_bb(ksq))) &
                b.getPieces(core::Color::Black, core::PieceType::Pawn);
  } else {
    checkers |= (bb::nw(bb::sq_bb(ksq)) | bb::ne(bb::sq_bb(ksq))) &
                b.getPieces(core::Color::White, core::PieceType::Pawn);
  }
  checkers |= bb::knight_attacks_from(ksq) & b.getPieces(them, core::PieceType::Knight);
  {
    const bb::Bitboard rays = magic::sliding_attacks(magic::Slider::Bishop, ksq, occ);
    checkers |= rays & (b.getPieces(them, core::PieceType::Bishop) |
                        b.getPieces(them, core::PieceType::Queen));
  }
  {
    const bb::Bitboard rays = magic::sliding_attacks(magic::Slider::Rook, ksq, occ);
    checkers |= rays & (b.getPieces(them, core::PieceType::Rook) |
                        b.getPieces(them, core::PieceType::Queen));
  }

  const int numCheckers = bb::popcount(checkers);

  // Königszüge (wie gehabt, mit korrekter Belegung)
  {
    const bb::Bitboard enemyNoK = b.getPieces(them) & ~b.getPieces(them, core::PieceType::King);
    const bb::Bitboard atk = bb::king_attacks_from(ksq);
    const bb::Bitboard fromBB = bb::sq_bb(ksq);

    for (bb::Bitboard caps = atk & enemyNoK; caps;) {
      core::Square to = bb::pop_lsb(caps);
      const bb::Bitboard occ2 = (occ & ~fromBB & ~bb::sq_bb(to));
      if (!attackedBy(b, to, them, occ2))
        emit(Move{ksq, to, core::PieceType::None, true, false, CastleSide::None});
    }
    for (bb::Bitboard quiet = atk & ~occ; quiet;) {
      core::Square to = bb::pop_lsb(quiet);
      const bb::Bitboard occ2 = (occ & ~fromBB);
      if (!attackedBy(b, to, them, occ2))
        emit(Move{ksq, to, core::PieceType::None, false, false, CastleSide::None});
    }
  }

  if (numCheckers >= 2) return;  // nur Königszüge erlaubt

  // Blocken/Schlagen eines einzelnen Checkers
  bb::Bitboard blockMask = 0ULL;
  if (numCheckers == 1) {
    const core::Square checkerSq = static_cast<core::Square>(bb::ctz64(checkers));
    blockMask = squares_between(ksq, checkerSq);
  }
  const bb::Bitboard evasionTargets = checkers | blockMask;

  // Maskierte Generierung für Nicht-Königszüge
  const SideSets our = side_sets(b, us);
  const SideSets opp = side_sets(b, them);

  // Pawns (ohne EP-Filter; EP separat unten)
  if (us == core::Color::White)
    genPawnMoves_T<core::Color::White>(
        b, st, occ, our, opp,
        [&](const Move& m) {
          if (m.from != ksq && (bb::sq_bb(m.to) & evasionTargets)) emit(m);
        },
        evasionTargets);
  else
    genPawnMoves_T<core::Color::Black>(
        b, st, occ, our, opp,
        [&](const Move& m) {
          if (m.from != ksq && (bb::sq_bb(m.to) & evasionTargets)) emit(m);
        },
        evasionTargets);

  genKnightMoves_T(
      b, our, opp, occ,
      [&](const Move& m) {
        if (bb::sq_bb(m.to) & evasionTargets) emit(m);
      },
      evasionTargets);
  genBishopMoves_T(
      our, opp, occ,
      [&](const Move& m) {
        if (bb::sq_bb(m.to) & evasionTargets) emit(m);
      },
      evasionTargets);
  genRookMoves_T(
      our, opp, occ,
      [&](const Move& m) {
        if (bb::sq_bb(m.to) & evasionTargets) emit(m);
      },
      evasionTargets);
  genQueenMoves_T(
      our, opp, occ,
      [&](const Move& m) {
        if (bb::sq_bb(m.to) & evasionTargets) emit(m);
      },
      evasionTargets);

  // EP-Evasion (separat & streng: König darf nach EP NICHT angegriffen sein)
  if (st.enPassantSquare != core::NO_SQUARE) {
    const core::Square epSq = st.enPassantSquare;
    const bb::Bitboard ep = bb::sq_bb(epSq);

    if (us == core::Color::White) {
      for (bb::Bitboard f = ((bb::sw(ep) | bb::se(ep)) & our.pawns); f;) {
        core::Square from = bb::pop_lsb(f);
        const core::Square capSq = static_cast<core::Square>((int)epSq - 8);
        bb::Bitboard occAfter = (occ & ~bb::sq_bb(from) & ~bb::sq_bb(capSq)) | bb::sq_bb(epSq);
        if (!attackedBy(b, ksq, them, occAfter))
          emit(Move{from, epSq, core::PieceType::None, true, true, CastleSide::None});
      }
    } else {
      for (bb::Bitboard f = ((bb::nw(ep) | bb::ne(ep)) & our.pawns); f;) {
        core::Square from = bb::pop_lsb(f);
        const core::Square capSq = static_cast<core::Square>((int)epSq + 8);
        bb::Bitboard occAfter = (occ & ~bb::sq_bb(from) & ~bb::sq_bb(capSq)) | bb::sq_bb(epSq);
        if (!attackedBy(b, ksq, them, occAfter))
          emit(Move{from, epSq, core::PieceType::None, true, true, CastleSide::None});
      }
    }
  }
}

// ---------------- Gemeinsamer Dispatcher mit Accept-Filter ----------------

struct AcceptAny {
  inline bool operator()(const Move&) const noexcept { return true; }
};
struct AcceptCaptures {
  inline bool operator()(const Move& m) const noexcept {
    return m.isCapture || m.promotion != PT::None;
  }
};

template <class Emit, class Accept>
inline void generate_all_regular(const Board& b, const GameState& st, Emit&& emit,
                                 Accept&& accept) noexcept {
  const Color side = st.sideToMove;
  const Color them = ~side;

  const bb::Bitboard occ = b.getAllPieces();
  const SideSets our = side_sets(b, side);
  const SideSets opp = side_sets(b, them);

  // Pins vorbereiten
  PinInfo pins;
  compute_pins(b, side, occ, pins);

  const bb::Bitboard kbb = our.king;
  const Square ksq = kbb ? static_cast<Square>(bb::ctz64(kbb)) : core::NO_SQUARE;

  auto gated_emit = [&](const Move& m) {
    if (m.from != ksq && (pins.pinned & bb::sq_bb(m.from))) {
      if (!(pins.allow_mask(m.from) & bb::sq_bb(m.to))) return;
    }
    if (!accept(m)) return;
    emit(m);
  };

  if (side == Color::White)
    genPawnMoves_T<Color::White>(b, st, occ, our, opp, gated_emit);
  else
    genPawnMoves_T<Color::Black>(b, st, occ, our, opp, gated_emit);

  genKnightMoves_T(b, our, opp, occ, gated_emit);
  genBishopMoves_T(our, opp, occ, gated_emit);
  genRookMoves_T(our, opp, occ, gated_emit);
  genQueenMoves_T(our, opp, occ, gated_emit);
  genKingMoves_T(b, st, side, our, opp, occ, gated_emit);
}

}  // namespace

// ---------------- Öffentliche APIs ----------------

void MoveGenerator::generatePseudoLegalMoves(const Board& b, const GameState& st,
                                             std::vector<model::Move>& out) const {
  if (out.capacity() < 128) out.reserve(128);
  out.clear();

  auto sink = [&](const Move& m) { out.push_back(m); };
  generate_all_regular(b, st, sink, AcceptAny{});
}

int MoveGenerator::generatePseudoLegalMoves(const Board& b, const GameState& st,
                                            engine::MoveBuffer& buf) {
  auto sink = [&](const Move& m) { buf.push_unchecked(m); };
  generate_all_regular(b, st, sink, AcceptAny{});
  return buf.n;
}

void MoveGenerator::generateCapturesOnly(const Board& b, const GameState& st,
                                         std::vector<model::Move>& out) const {
  if (out.capacity() < 64) out.reserve(64);
  out.clear();

  auto sink = [&](const Move& m) { out.push_back(m); };
  generate_all_regular(b, st, sink, AcceptCaptures{});
}

int MoveGenerator::generateCapturesOnly(const Board& b, const GameState& st,
                                        engine::MoveBuffer& buf) {
  auto sink = [&](const Move& m) { buf.push_unchecked(m); };
  generate_all_regular(b, st, sink, AcceptCaptures{});
  return buf.n;
}

void MoveGenerator::generateEvasions(const Board& b, const GameState& st,
                                     std::vector<model::Move>& out) const {
  if (out.capacity() < 48) out.reserve(48);
  out.clear();

  // Pins weiterhin respektieren (wie im Original): Wrap um Evasion-Emitter
  const Color side = st.sideToMove;
  const bb::Bitboard occ = b.getAllPieces();

  PinInfo pins;
  compute_pins(b, side, occ, pins);

  const bb::Bitboard kbb = b.getPieces(side, PT::King);
  const Square ksq = kbb ? static_cast<Square>(bb::ctz64(kbb)) : core::NO_SQUARE;

  auto emitV = [&](const Move& m) {
    if (m.from != ksq && (pins.pinned & bb::sq_bb(m.from))) {
      if (!(pins.allow_mask(m.from) & bb::sq_bb(m.to))) return;
    }
    out.push_back(m);
  };

  generateEvasions_T(b, st, emitV);
}

int MoveGenerator::generateEvasions(const Board& b, const GameState& st, engine::MoveBuffer& buf) {
  const Color side = st.sideToMove;
  const bb::Bitboard occ = b.getAllPieces();

  PinInfo pins;
  compute_pins(b, side, occ, pins);

  const bb::Bitboard kbb = b.getPieces(side, PT::King);
  const Square ksq = kbb ? static_cast<Square>(bb::ctz64(kbb)) : core::NO_SQUARE;

  auto emitB = [&](const Move& m) {
    if (m.from != ksq && (pins.pinned & bb::sq_bb(m.from))) {
      if (!(pins.allow_mask(m.from) & bb::sq_bb(m.to))) return;
    }
    buf.push_unchecked(m);
  };

  generateEvasions_T(b, st, emitB);
  return buf.n;
}

}  // namespace lilia::model
