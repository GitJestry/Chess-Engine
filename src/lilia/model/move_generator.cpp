#include "lilia/model/move_generator.hpp"

#include "lilia/model/core/magic.hpp"

namespace lilia::model {

namespace {

// Feld von Seite "by" angegriffen?
inline bool attackedBy(const Board& board, core::Square sq, core::Color by) {
  using PT = core::PieceType;
  const auto target = bb::sq_bb(sq);
  if (by == core::Color::White) {
    if ((bb::sw(target) | bb::se(target)) & board.getPieces(core::Color::White, PT::Pawn))
      return true;
  } else {
    if ((bb::nw(target) | bb::ne(target)) & board.getPieces(core::Color::Black, PT::Pawn))
      return true;
  }
  if (bb::knight_attacks_from(sq) & board.getPieces(by, PT::Knight)) return true;

  const auto occ = board.getAllPieces();
  if (magic::sliding_attacks(magic::Slider::Bishop, sq, occ) &
      (board.getPieces(by, PT::Bishop) | board.getPieces(by, PT::Queen)))
    return true;
  if (magic::sliding_attacks(magic::Slider::Rook, sq, occ) &
      (board.getPieces(by, PT::Rook) | board.getPieces(by, PT::Queen)))
    return true;

  if (bb::king_attacks_from(sq) & board.getPieces(by, PT::King)) return true;
  return false;
}

// Squares strictly between a und b
inline bb::Bitboard squares_between(core::Square a, core::Square b) {
  bb::Bitboard mask = 0;
  int ai = (int)a, bi = (int)b;
  int d = bi - ai, step = 0;
  if (d % 9 == 0)
    step = (d > 0 ? 9 : -9);
  else if (d % 7 == 0)
    step = (d > 0 ? 7 : -7);
  else if (ai / 8 == bi / 8)
    step = (d > 0 ? 1 : -1);
  else if ((ai % 8) == (bi % 8))
    step = (d > 0 ? 8 : -8);
  else
    return 0;
  for (int cur = ai + step; cur != bi; cur += step)
    mask |= bb::sq_bb(static_cast<core::Square>(cur));
  return mask;
}

// ---------------- Pin-Infos ----------------

struct PinInfo {
  bb::Bitboard pinned = 0;
  std::array<bb::Bitboard, 64> allow{};
};

inline void compute_pins(const Board& b, core::Color us, PinInfo& out) {
  using PT = core::PieceType;
  out.pinned = 0;
  out.allow.fill(~0ULL);

  const bb::Bitboard kbb = b.getPieces(us, PT::King);
  if (!kbb) return;
  const core::Square ksq = static_cast<core::Square>(bb::ctz64(kbb));
  const bb::Bitboard occ = b.getAllPieces();

  auto mark_if_pinned = [&](core::Square pinnerSq) {
    const bb::Bitboard between = squares_between(ksq, pinnerSq);
    if (!between) return;
    const bb::Bitboard blockers = between & occ;
    if (bb::popcount(blockers) != 1) return;
    const bb::Bitboard ours = blockers & b.getPieces(us);
    if (!ours) return;
    const core::Square pinnedSq = static_cast<core::Square>(bb::ctz64(ours));
    out.pinned |= ours;
    out.allow[(int)pinnedSq] = between | bb::sq_bb(pinnerSq);
  };

  for (bb::Bitboard s = b.getPieces(~us, PT::Bishop) | b.getPieces(~us, PT::Queen); s;) {
    const core::Square sq = bb::pop_lsb(s);
    if (squares_between(ksq, sq)) mark_if_pinned(sq);
  }
  for (bb::Bitboard s = b.getPieces(~us, PT::Rook) | b.getPieces(~us, PT::Queen); s;) {
    const core::Square sq = bb::pop_lsb(s);
    if (squares_between(ksq, sq)) mark_if_pinned(sq);
  }
}

// --------- schneller EP-Legalitätscheck ---------
// Nur horizontale (Rook/Queen) Entblößung ist neu möglich.
// Prüft nach virtueller Ausführung, ob der König durch R/T angegriffen ist.
inline bool ep_is_legal_fast(const Board& b, core::Color side, core::Square from,
                             core::Square to) {  // [EP-FAST]
  using PT = core::PieceType;
  const bb::Bitboard kbb = b.getPieces(side, PT::King);
  if (!kbb) return false;
  const core::Square ksq = static_cast<core::Square>(bb::ctz64(kbb));

  // nur relevant wenn König und EP-Konstellation auf derselben Reihe (5. für Weiß, 4. für Schwarz)
  if (bb::rank_of(ksq) != bb::rank_of(from)) return true;

  const core::Square capSq = (side == core::Color::White) ? static_cast<core::Square>(to - 8)
                                                          : static_cast<core::Square>(to + 8);

  bb::Bitboard occ = b.getAllPieces();
  occ &= ~bb::sq_bb(from);   // mover verlässt from
  occ &= ~bb::sq_bb(capSq);  // geschlagener Bauer verschwindet
  occ |= bb::sq_bb(to);      // mover landet auf to

  const bb::Bitboard sliders = b.getPieces(~side, PT::Rook) | b.getPieces(~side, PT::Queen);
  const bb::Bitboard rays = magic::sliding_attacks(magic::Slider::Rook, ksq, occ);
  return (rays & sliders) == 0;  // legal, wenn kein R/T jetzt auf Königslinie sitzt
}

// ---------------- Template-Generatoren ----------------

template <class Emit>
inline void genPawnMoves_T(const Board& board, const GameState& st, core::Color side, Emit&& emit) {
  const bb::Bitboard occ = board.getAllPieces();
  const bb::Bitboard empty = ~occ;

  const bb::Bitboard enemyAll = board.getPieces(~side);
  const bb::Bitboard enemyKing = board.getPieces(~side, core::PieceType::King);
  const bb::Bitboard them = enemyAll & ~enemyKing;

  bb::Bitboard pawns = board.getPieces(side, core::PieceType::Pawn);

  if (side == core::Color::White) {
    bb::Bitboard one = bb::north(pawns) & empty;
    bb::Bitboard dbl = bb::north(one & bb::RANK_3) & empty;

    bb::Bitboard promoPush = one & bb::RANK_8;
    bb::Bitboard quietPush = one & ~bb::RANK_8;

    bb::Bitboard capL = (bb::nw(pawns) & them) & ~bb::RANK_8;
    bb::Bitboard capR = (bb::ne(pawns) & them) & ~bb::RANK_8;

    for (bb::Bitboard q = quietPush; q;) {
      core::Square to = bb::pop_lsb(q);
      emit(Move{static_cast<core::Square>(to - 8), to, core::PieceType::None, false, false,
                CastleSide::None});
    }
    for (bb::Bitboard d = dbl; d;) {
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
    for (bb::Bitboard pp = promoPush; pp;) {
      core::Square to = bb::pop_lsb(pp);
      core::Square from = static_cast<core::Square>(to - 8);
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight})
        emit(Move{from, to, pt, false, false, CastleSide::None});
    }
    bb::Bitboard capLP = (bb::nw(pawns) & them) & bb::RANK_8;
    bb::Bitboard capRP = (bb::ne(pawns) & them) & bb::RANK_8;
    for (bb::Bitboard c = capLP; c;) {
      core::Square to = bb::pop_lsb(c);
      core::Square from = static_cast<core::Square>(to - 7);
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight})
        emit(Move{from, to, pt, true, false, CastleSide::None});
    }
    for (bb::Bitboard c = capRP; c;) {
      core::Square to = bb::pop_lsb(c);
      core::Square from = static_cast<core::Square>(to - 9);
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight})
        emit(Move{from, to, pt, true, false, CastleSide::None});
    }
  } else {
    bb::Bitboard one = bb::south(pawns) & empty;
    bb::Bitboard dbl = bb::south(one & bb::RANK_6) & empty;

    bb::Bitboard promoPush = one & bb::RANK_1;
    bb::Bitboard quietPush = one & ~bb::RANK_1;

    bb::Bitboard capL = (bb::se(pawns) & them) & ~bb::RANK_1;
    bb::Bitboard capR = (bb::sw(pawns) & them) & ~bb::RANK_1;

    for (bb::Bitboard q = quietPush; q;) {
      core::Square to = bb::pop_lsb(q);
      emit(Move{static_cast<core::Square>(to + 8), to, core::PieceType::None, false, false,
                CastleSide::None});
    }
    for (bb::Bitboard d = dbl; d;) {
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
    for (bb::Bitboard pp = promoPush; pp;) {
      core::Square to = bb::pop_lsb(pp);
      core::Square from = static_cast<core::Square>(to + 8);
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight})
        emit(Move{from, to, pt, false, false, CastleSide::None});
    }
    bb::Bitboard capLP = (bb::se(pawns) & them) & bb::RANK_1;
    bb::Bitboard capRP = (bb::sw(pawns) & them) & bb::RANK_1;
    for (bb::Bitboard c = capLP; c;) {
      core::Square to = bb::pop_lsb(c);
      core::Square from = static_cast<core::Square>(to + 7);
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight})
        emit(Move{from, to, pt, true, false, CastleSide::None});
    }
    for (bb::Bitboard c = capRP; c;) {
      core::Square to = bb::pop_lsb(c);
      core::Square from = static_cast<core::Square>(to + 9);
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight})
        emit(Move{from, to, pt, true, false, CastleSide::None});
    }
  }

  // En passant – jetzt mit schnellem Legalitätscheck
  if (st.enPassantSquare != core::NO_SQUARE) {
    const bb::Bitboard ep = bb::sq_bb(st.enPassantSquare);
    if (side == core::Color::White) {
      bb::Bitboard froms =
          (bb::sw(ep) | bb::se(ep)) & board.getPieces(core::Color::White, core::PieceType::Pawn);
      for (bb::Bitboard f = froms; f;) {
        core::Square from = bb::pop_lsb(f);
        core::Square to = st.enPassantSquare;
        if (ep_is_legal_fast(board, side, from, to)) {  // [EP-FAST]
          emit(Move{from, to, core::PieceType::None, true, true, CastleSide::None});
        }
      }
    } else {
      bb::Bitboard froms =
          (bb::nw(ep) | bb::ne(ep)) & board.getPieces(core::Color::Black, core::PieceType::Pawn);
      for (bb::Bitboard f = froms; f;) {
        core::Square from = bb::pop_lsb(f);
        core::Square to = st.enPassantSquare;
        if (ep_is_legal_fast(board, side, from, to)) {  // [EP-FAST]
          emit(Move{from, to, core::PieceType::None, true, true, CastleSide::None});
        }
      }
    }
  }
}

template <class Emit>
inline void genKnightMoves_T(const Board& board, core::Color side, Emit&& emit) {
  const bb::Bitboard knights = board.getPieces(side, core::PieceType::Knight);
  const bb::Bitboard enemyK = board.getPieces(~side, core::PieceType::King);
  const bb::Bitboard enemyNoK = board.getPieces(~side) & ~enemyK;
  const bb::Bitboard occ = board.getAllPieces();

  for (bb::Bitboard n = knights; n;) {
    core::Square from = bb::pop_lsb(n);
    bb::Bitboard atk = bb::knight_attacks_from(from);
    for (bb::Bitboard caps = atk & enemyNoK; caps;) {
      core::Square to = bb::pop_lsb(caps);
      emit(Move{from, to, core::PieceType::None, true, false, CastleSide::None});
    }
    for (bb::Bitboard quiet = atk & ~occ; quiet;) {
      core::Square to = bb::pop_lsb(quiet);
      emit(Move{from, to, core::PieceType::None, false, false, CastleSide::None});
    }
  }
}

template <class Emit>
inline void genBishopMoves_T(const Board& board, core::Color side, Emit&& emit) {
  const bb::Bitboard bishops = board.getPieces(side, core::PieceType::Bishop);
  const bb::Bitboard occ = board.getAllPieces();
  const bb::Bitboard enemyK = board.getPieces(~side, core::PieceType::King);
  const bb::Bitboard enemyNoK = board.getPieces(~side) & ~enemyK;

  for (bb::Bitboard b = bishops; b;) {
    core::Square from = bb::pop_lsb(b);
    bb::Bitboard atk = magic::sliding_attacks(magic::Slider::Bishop, from, occ);
    for (bb::Bitboard caps = atk & enemyNoK; caps;) {
      core::Square to = bb::pop_lsb(caps);
      emit(Move{from, to, core::PieceType::None, true, false, CastleSide::None});
    }
    for (bb::Bitboard quiet = atk & ~occ; quiet;) {
      core::Square to = bb::pop_lsb(quiet);
      emit(Move{from, to, core::PieceType::None, false, false, CastleSide::None});
    }
  }
}

template <class Emit>
inline void genRookMoves_T(const Board& board, core::Color side, Emit&& emit) {
  const bb::Bitboard rooks = board.getPieces(side, core::PieceType::Rook);
  const bb::Bitboard occ = board.getAllPieces();
  const bb::Bitboard enemyK = board.getPieces(~side, core::PieceType::King);
  const bb::Bitboard enemyNoK = board.getPieces(~side) & ~enemyK;

  for (bb::Bitboard r = rooks; r;) {
    core::Square from = bb::pop_lsb(r);
    bb::Bitboard atk = magic::sliding_attacks(magic::Slider::Rook, from, occ);
    for (bb::Bitboard caps = atk & enemyNoK; caps;) {
      core::Square to = bb::pop_lsb(caps);
      emit(Move{from, to, core::PieceType::None, true, false, CastleSide::None});
    }
    for (bb::Bitboard quiet = atk & ~occ; quiet;) {
      core::Square to = bb::pop_lsb(quiet);
      emit(Move{from, to, core::PieceType::None, false, false, CastleSide::None});
    }
  }
}

template <class Emit>
inline void genQueenMoves_T(const Board& board, core::Color side, Emit&& emit) {
  const bb::Bitboard queens = board.getPieces(side, core::PieceType::Queen);
  const bb::Bitboard occ = board.getAllPieces();
  const bb::Bitboard enemyK = board.getPieces(~side, core::PieceType::King);
  const bb::Bitboard enemyNoK = board.getPieces(~side) & ~enemyK;

  for (bb::Bitboard q = queens; q;) {
    core::Square from = bb::pop_lsb(q);
    bb::Bitboard atk = magic::sliding_attacks(magic::Slider::Bishop, from, occ) |
                       magic::sliding_attacks(magic::Slider::Rook, from, occ);
    for (bb::Bitboard caps = atk & enemyNoK; caps;) {
      core::Square to = bb::pop_lsb(caps);
      emit(Move{from, to, core::PieceType::None, true, false, CastleSide::None});
    }
    for (bb::Bitboard quiet = atk & ~occ; quiet;) {
      core::Square to = bb::pop_lsb(quiet);
      emit(Move{from, to, core::PieceType::None, false, false, CastleSide::None});
    }
  }
}

template <class Emit>
inline void genKingMoves_T(const Board& board, const GameState& st, core::Color side, Emit&& emit) {
  const bb::Bitboard king = board.getPieces(side, core::PieceType::King);
  if (!king) return;
  const core::Square from = static_cast<core::Square>(bb::ctz64(king));

  const bb::Bitboard occ = board.getAllPieces();
  const bb::Bitboard enemyK = board.getPieces(~side, core::PieceType::King);
  const bb::Bitboard enemyNoK = board.getPieces(~side) & ~enemyK;

  const bb::Bitboard atk = bb::king_attacks_from(from);

  for (bb::Bitboard caps = atk & enemyNoK; caps;) {
    core::Square to = bb::pop_lsb(caps);
    emit(Move{from, to, core::PieceType::None, true, false, CastleSide::None});
  }
  for (bb::Bitboard quiet = atk & ~occ; quiet;) {
    core::Square to = bb::pop_lsb(quiet);
    emit(Move{from, to, core::PieceType::None, false, false, CastleSide::None});
  }

  const core::Color enemySide = ~side;

  if (side == core::Color::White) {
    if ((st.castlingRights & bb::Castling::WK) &&
        (board.getPieces(core::Color::White, core::PieceType::Rook) & bb::sq_bb(bb::H1)) &&
        !(occ & (bb::sq_bb(core::Square{5}) | bb::sq_bb(core::Square{6})))) {
      if (!attackedBy(board, core::Square{4}, enemySide) &&
          !attackedBy(board, core::Square{5}, enemySide) &&
          !attackedBy(board, core::Square{6}, enemySide)) {
        emit(Move{bb::E1, core::Square{6}, core::PieceType::None, false, false,
                  CastleSide::KingSide});
      }
    }
    if ((st.castlingRights & bb::Castling::WQ) &&
        (board.getPieces(core::Color::White, core::PieceType::Rook) & bb::sq_bb(bb::A1)) &&
        !(occ &
          (bb::sq_bb(core::Square{3}) | bb::sq_bb(core::Square{2}) | bb::sq_bb(core::Square{1})))) {
      if (!attackedBy(board, core::Square{4}, enemySide) &&
          !attackedBy(board, core::Square{3}, enemySide) &&
          !attackedBy(board, core::Square{2}, enemySide)) {
        emit(Move{bb::E1, core::Square{2}, core::PieceType::None, false, false,
                  CastleSide::QueenSide});
      }
    }
  } else {
    if ((st.castlingRights & bb::Castling::BK) &&
        (board.getPieces(core::Color::Black, core::PieceType::Rook) & bb::sq_bb(bb::H8)) &&
        !(occ & (bb::sq_bb(core::Square{61}) | bb::sq_bb(core::Square{62})))) {
      if (!attackedBy(board, core::Square{60}, enemySide) &&
          !attackedBy(board, core::Square{61}, enemySide) &&
          !attackedBy(board, core::Square{62}, enemySide)) {
        emit(Move{bb::E8, core::Square{62}, core::PieceType::None, false, false,
                  CastleSide::KingSide});
      }
    }
    if ((st.castlingRights & bb::Castling::BQ) &&
        (board.getPieces(core::Color::Black, core::PieceType::Rook) & bb::sq_bb(bb::A8)) &&
        !(occ & (bb::sq_bb(core::Square{59}) | bb::sq_bb(core::Square{58}) |
                 bb::sq_bb(core::Square{57})))) {
      if (!attackedBy(board, core::Square{60}, enemySide) &&
          !attackedBy(board, core::Square{59}, enemySide) &&
          !attackedBy(board, core::Square{58}, enemySide)) {
        emit(Move{bb::E8, core::Square{58}, core::PieceType::None, false, false,
                  CastleSide::QueenSide});
      }
    }
  }
}

// Evasions-Generator mit Emit
template <class Emit>
inline void generateEvasions_T(const Board& b, const GameState& st, Emit&& emit) {
  using PT = core::PieceType;
  const core::Color us = st.sideToMove;
  const core::Color them = ~us;
  const bb::Bitboard kbb = b.getPieces(us, PT::King);
  if (!kbb) return;
  const core::Square ksq = static_cast<core::Square>(bb::ctz64(kbb));

  const auto occ = b.getAllPieces();

  bb::Bitboard checkers = 0;
  if (us == core::Color::White) {
    checkers |= (bb::sw(bb::sq_bb(ksq)) | bb::se(bb::sq_bb(ksq))) &
                b.getPieces(core::Color::Black, PT::Pawn);
  } else {
    checkers |= (bb::nw(bb::sq_bb(ksq)) | bb::ne(bb::sq_bb(ksq))) &
                b.getPieces(core::Color::White, PT::Pawn);
  }
  checkers |= bb::knight_attacks_from(ksq) & b.getPieces(them, PT::Knight);
  for (bb::Bitboard s = b.getPieces(them, PT::Bishop) | b.getPieces(them, PT::Queen); s;) {
    const core::Square sq = bb::pop_lsb(s);
    if (magic::sliding_attacks(magic::Slider::Bishop, sq, occ) & bb::sq_bb(ksq))
      checkers |= bb::sq_bb(sq);
  }
  for (bb::Bitboard s = b.getPieces(them, PT::Rook) | b.getPieces(them, PT::Queen); s;) {
    const core::Square sq = bb::pop_lsb(s);
    if (magic::sliding_attacks(magic::Slider::Rook, sq, occ) & bb::sq_bb(ksq))
      checkers |= bb::sq_bb(sq);
  }

  const int numCheckers = bb::popcount(checkers);

  {
    const bb::Bitboard enemyK = b.getPieces(them, PT::King);
    const bb::Bitboard enemyNoK = b.getPieces(them) & ~enemyK;
    const bb::Bitboard atk = bb::king_attacks_from(ksq);
    const bb::Bitboard all = b.getAllPieces();

    for (bb::Bitboard caps = atk & enemyNoK; caps;) {
      const core::Square to = bb::pop_lsb(caps);
      if (!attackedBy(b, to, them)) emit(Move{ksq, to, PT::None, true, false, CastleSide::None});
    }
    for (bb::Bitboard quiet = atk & ~all; quiet;) {
      const core::Square to = bb::pop_lsb(quiet);
      if (!attackedBy(b, to, them)) emit(Move{ksq, to, PT::None, false, false, CastleSide::None});
    }
  }
  if (numCheckers >= 2) return;

  bb::Bitboard blockMask = 0;
  if (numCheckers == 1) {
    const core::Square checkerSq = static_cast<core::Square>(bb::ctz64(checkers));
    const bb::Bitboard checkerBB = bb::sq_bb(checkerSq);
    if (checkerBB & (b.getPieces(them, PT::Bishop) | b.getPieces(them, PT::Queen)))
      blockMask |= squares_between(ksq, checkerSq);
    if (checkerBB & (b.getPieces(them, PT::Rook) | b.getPieces(them, PT::Queen)))
      blockMask |= squares_between(ksq, checkerSq);
  }
  const bb::Bitboard evasionTargets = checkers | blockMask;

  auto sink = [&](const Move& m) {
    if (m.from == ksq) return;
    const bool isEP = m.isEnPassant;
    const bool hits = (bb::sq_bb(m.to) & evasionTargets) != 0;
    if (hits || isEP) emit(m);
  };

  genPawnMoves_T(b, st, us, sink);
  genKnightMoves_T(b, us, sink);
  genBishopMoves_T(b, us, sink);
  genRookMoves_T(b, us, sink);
  genQueenMoves_T(b, us, sink);
}

}  // namespace

// ---------------- Öffentliche APIs (mit Pin-Filter + EP-Check) ----------------

void MoveGenerator::generatePseudoLegalMoves(const Board& b, const GameState& st,
                                             std::vector<model::Move>& out) const {
  if (out.capacity() < 128) out.reserve(128);
  out.clear();

  PinInfo pins;
  compute_pins(b, st.sideToMove, pins);

  const core::Color side = st.sideToMove;
  const bb::Bitboard kbb = b.getPieces(side, core::PieceType::King);
  const core::Square ksq = kbb ? static_cast<core::Square>(bb::ctz64(kbb)) : core::NO_SQUARE;

  auto emitV = [&](const Move& m) {
    // [PIN] König nie masken, alle anderen strikt filtern – jetzt auch EP
    if (m.from != ksq && (pins.pinned & bb::sq_bb(m.from))) {
      if ((pins.allow[(int)m.from] & bb::sq_bb(m.to)) == 0) return;
    }
    out.push_back(m);
  };

  genPawnMoves_T(b, st, side, emitV);
  genKnightMoves_T(b, side, emitV);
  genBishopMoves_T(b, side, emitV);
  genRookMoves_T(b, side, emitV);
  genQueenMoves_T(b, side, emitV);
  genKingMoves_T(b, st, side, emitV);
}

int MoveGenerator::generatePseudoLegalMoves(const Board& b, const GameState& st,
                                            engine::MoveBuffer& buf) {
  PinInfo pins;
  compute_pins(b, st.sideToMove, pins);

  const core::Color side = st.sideToMove;
  const bb::Bitboard kbb = b.getPieces(side, core::PieceType::King);
  const core::Square ksq = kbb ? static_cast<core::Square>(bb::ctz64(kbb)) : core::NO_SQUARE;

  auto emitB = [&](const Move& m) {
    if (m.from != ksq && (pins.pinned & bb::sq_bb(m.from))) {
      if ((pins.allow[(int)m.from] & bb::sq_bb(m.to)) == 0) return;
    }
    buf.push_unchecked(m);
  };

  genPawnMoves_T(b, st, side, emitB);
  genKnightMoves_T(b, side, emitB);
  genBishopMoves_T(b, side, emitB);
  genRookMoves_T(b, side, emitB);
  genQueenMoves_T(b, side, emitB);
  genKingMoves_T(b, st, side, emitB);
  return buf.n;
}

void MoveGenerator::generateCapturesOnly(const Board& b, const GameState& st,
                                         std::vector<model::Move>& out) const {
  if (out.capacity() < 64) out.reserve(64);
  out.clear();

  PinInfo pins;
  compute_pins(b, st.sideToMove, pins);

  const core::Color side = st.sideToMove;
  const bb::Bitboard kbb = b.getPieces(side, core::PieceType::King);
  const core::Square ksq = kbb ? static_cast<core::Square>(bb::ctz64(kbb)) : core::NO_SQUARE;

  auto emitV = [&](const Move& m) {
    if (!m.isCapture && m.promotion == core::PieceType::None) return;
    if (m.from != ksq && (pins.pinned & bb::sq_bb(m.from))) {
      if ((pins.allow[(int)m.from] & bb::sq_bb(m.to)) == 0) return;
    }
    out.push_back(m);
  };

  genPawnMoves_T(b, st, side, emitV);
  genKnightMoves_T(b, side, emitV);
  genBishopMoves_T(b, side, emitV);
  genRookMoves_T(b, side, emitV);
  genQueenMoves_T(b, side, emitV);
  genKingMoves_T(b, st, side, emitV);
}

int MoveGenerator::generateCapturesOnly(const Board& b, const GameState& st,
                                        engine::MoveBuffer& buf) {
  PinInfo pins;
  compute_pins(b, st.sideToMove, pins);

  const core::Color side = st.sideToMove;
  const bb::Bitboard kbb = b.getPieces(side, core::PieceType::King);
  const core::Square ksq = kbb ? static_cast<core::Square>(bb::ctz64(kbb)) : core::NO_SQUARE;

  auto emitB = [&](const Move& m) {
    if (!m.isCapture && m.promotion == core::PieceType::None) return;
    if (m.from != ksq && (pins.pinned & bb::sq_bb(m.from))) {
      if ((pins.allow[(int)m.from] & bb::sq_bb(m.to)) == 0) return;
    }
    buf.push_unchecked(m);
  };

  genPawnMoves_T(b, st, side, emitB);
  genKnightMoves_T(b, side, emitB);
  genBishopMoves_T(b, side, emitB);
  genRookMoves_T(b, side, emitB);
  genQueenMoves_T(b, side, emitB);
  genKingMoves_T(b, st, side, emitB);
  return buf.n;
}

void MoveGenerator::generateEvasions(const Board& b, const GameState& st,
                                     std::vector<model::Move>& out) const {
  if (out.capacity() < 48) out.reserve(48);
  out.clear();

  PinInfo pins;
  compute_pins(b, st.sideToMove, pins);

  const core::Color side = st.sideToMove;
  const bb::Bitboard kbb = b.getPieces(side, core::PieceType::King);
  const core::Square ksq = kbb ? static_cast<core::Square>(bb::ctz64(kbb)) : core::NO_SQUARE;

  auto emitV = [&](const Move& m) {
    if (m.from != ksq && (pins.pinned & bb::sq_bb(m.from))) {
      if ((pins.allow[(int)m.from] & bb::sq_bb(m.to)) == 0) return;
    }
    out.push_back(m);
  };
  generateEvasions_T(b, st, emitV);
}

int MoveGenerator::generateEvasions(const Board& b, const GameState& st, engine::MoveBuffer& buf) {
  PinInfo pins;
  compute_pins(b, st.sideToMove, pins);

  const core::Color side = st.sideToMove;
  const bb::Bitboard kbb = b.getPieces(side, core::PieceType::King);
  const core::Square ksq = kbb ? static_cast<core::Square>(bb::ctz64(kbb)) : core::NO_SQUARE;

  auto emitB = [&](const Move& m) {
    if (m.from != ksq && (pins.pinned & bb::sq_bb(m.from))) {
      if ((pins.allow[(int)m.from] & bb::sq_bb(m.to)) == 0) return;
    }
    buf.push_unchecked(m);
  };
  generateEvasions_T(b, st, emitB);
  return buf.n;
}

}  // namespace lilia::model
