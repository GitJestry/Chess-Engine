#include "lilia/model/move_generator.hpp"

#include "lilia/model/core/magic.hpp"

namespace lilia::model {

void MoveGenerator::generatePseudoLegalMoves(const Board& b, const GameState& st,
                                             std::vector<model::Move>& out) const {
  if (out.capacity() < 128) out.reserve(128);
  out.clear();

  const core::Color side = st.sideToMove;

  genPawnMoves(b, st, side, out);
  genKnightMoves(b, side, out);
  genBishopMoves(b, side, out);
  genRookMoves(b, side, out);
  genQueenMoves(b, side, out);
  genKingMoves(b, st, side, out);
}

// ------------------------ Pawns ------------------------

void MoveGenerator::genPawnMoves(const Board& board, const GameState& st, core::Color side,
                                 std::vector<Move>& out) const {
  const bb::Bitboard occ = board.getAllPieces();
  const bb::Bitboard empty = ~occ;

  // >>> Kein Königscapture: Gegner OHNE König
  const bb::Bitboard enemyAll = board.getPieces(~side);
  const bb::Bitboard enemyKing = board.getPieces(~side, core::PieceType::King);
  const bb::Bitboard them = enemyAll & ~enemyKing;

  bb::Bitboard pawns = board.getPieces(side, core::PieceType::Pawn);

  if (side == core::Color::White) {
    // 1-Step
    bb::Bitboard one = bb::north(pawns) & empty;

    // 2-Step (aus dem 1-Schritt ableiten; Zwischenfeld garantiert leer)
    bb::Bitboard dbl = bb::north(one & bb::RANK_3) & empty;

    // promo pushes / quiet pushes
    bb::Bitboard promoPush = one & bb::RANK_8;
    bb::Bitboard quietPush = one & ~bb::RANK_8;

    // Captures (nicht-Promo)
    bb::Bitboard capL = (bb::nw(pawns) & them) & ~bb::RANK_8;
    bb::Bitboard capR = (bb::ne(pawns) & them) & ~bb::RANK_8;

    // Pushes
    for (bb::Bitboard q = quietPush; q;) {
      core::Square to = bb::pop_lsb(q);
      out.push_back({static_cast<core::Square>(to - 8), to, core::PieceType::None, false, false,
                     CastleSide::None});
    }
    for (bb::Bitboard d = dbl; d;) {
      core::Square to = bb::pop_lsb(d);
      out.push_back({static_cast<core::Square>(to - 16), to, core::PieceType::None, false, false,
                     CastleSide::None});
    }

    // Captures
    for (bb::Bitboard c = capL; c;) {
      core::Square to = bb::pop_lsb(c);
      out.push_back({static_cast<core::Square>(to - 7), to, core::PieceType::None, true, false,
                     CastleSide::None});
    }
    for (bb::Bitboard c = capR; c;) {
      core::Square to = bb::pop_lsb(c);
      out.push_back({static_cast<core::Square>(to - 9), to, core::PieceType::None, true, false,
                     CastleSide::None});
    }

    // Promo pushes
    for (bb::Bitboard pp = promoPush; pp;) {
      core::Square to = bb::pop_lsb(pp);
      core::Square from = static_cast<core::Square>(to - 8);
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight}) {
        out.push_back({from, to, pt, false, false, CastleSide::None});
      }
    }
    // Promo captures
    bb::Bitboard capLP = (bb::nw(pawns) & them) & bb::RANK_8;
    bb::Bitboard capRP = (bb::ne(pawns) & them) & bb::RANK_8;
    for (bb::Bitboard c = capLP; c;) {
      core::Square to = bb::pop_lsb(c);
      core::Square from = static_cast<core::Square>(to - 7);
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight}) {
        out.push_back({from, to, pt, true, false, CastleSide::None});
      }
    }
    for (bb::Bitboard c = capRP; c;) {
      core::Square to = bb::pop_lsb(c);
      core::Square from = static_cast<core::Square>(to - 9);
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight}) {
        out.push_back({from, to, pt, true, false, CastleSide::None});
      }
    }
  } else {
    // 1-Step
    bb::Bitboard one = bb::south(pawns) & empty;

    // 2-Step
    bb::Bitboard dbl = bb::south(one & bb::RANK_6) & empty;

    // promo pushes / quiet pushes
    bb::Bitboard promoPush = one & bb::RANK_1;
    bb::Bitboard quietPush = one & ~bb::RANK_1;

    // Captures (nicht-Promo)
    bb::Bitboard capL = (bb::se(pawns) & them) & ~bb::RANK_1;
    bb::Bitboard capR = (bb::sw(pawns) & them) & ~bb::RANK_1;

    // Pushes
    for (bb::Bitboard q = quietPush; q;) {
      core::Square to = bb::pop_lsb(q);
      out.push_back({static_cast<core::Square>(to + 8), to, core::PieceType::None, false, false,
                     CastleSide::None});
    }
    for (bb::Bitboard d = dbl; d;) {
      core::Square to = bb::pop_lsb(d);
      out.push_back({static_cast<core::Square>(to + 16), to, core::PieceType::None, false, false,
                     CastleSide::None});
    }

    // Captures
    for (bb::Bitboard c = capL; c;) {
      core::Square to = bb::pop_lsb(c);
      out.push_back({static_cast<core::Square>(to + 7), to, core::PieceType::None, true, false,
                     CastleSide::None});
    }
    for (bb::Bitboard c = capR; c;) {
      core::Square to = bb::pop_lsb(c);
      out.push_back({static_cast<core::Square>(to + 9), to, core::PieceType::None, true, false,
                     CastleSide::None});
    }

    // Promo pushes
    for (bb::Bitboard pp = promoPush; pp;) {
      core::Square to = bb::pop_lsb(pp);
      core::Square from = static_cast<core::Square>(to + 8);
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight}) {
        out.push_back({from, to, pt, false, false, CastleSide::None});
      }
    }
    // Promo captures
    bb::Bitboard capLP = (bb::se(pawns) & them) & bb::RANK_1;
    bb::Bitboard capRP = (bb::sw(pawns) & them) & bb::RANK_1;
    for (bb::Bitboard c = capLP; c;) {
      core::Square to = bb::pop_lsb(c);
      core::Square from = static_cast<core::Square>(to + 7);
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight}) {
        out.push_back({from, to, pt, true, false, CastleSide::None});
      }
    }
    for (bb::Bitboard c = capRP; c;) {
      core::Square to = bb::pop_lsb(c);
      core::Square from = static_cast<core::Square>(to + 9);
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight}) {
        out.push_back({from, to, pt, true, false, CastleSide::None});
      }
    }
  }

  // En passant (pseudolegal – Pins filtert später der Legality-Check)
  if (st.enPassantSquare != core::NO_SQUARE) {
    const bb::Bitboard ep = bb::sq_bb(st.enPassantSquare);
    if (side == core::Color::White) {
      bb::Bitboard froms =
          (bb::sw(ep) | bb::se(ep)) & board.getPieces(core::Color::White, core::PieceType::Pawn);
      for (bb::Bitboard f = froms; f;) {
        core::Square from = bb::pop_lsb(f);
        out.push_back(
            {from, st.enPassantSquare, core::PieceType::None, true, true, CastleSide::None});
      }
    } else {
      bb::Bitboard froms =
          (bb::nw(ep) | bb::ne(ep)) & board.getPieces(core::Color::Black, core::PieceType::Pawn);
      for (bb::Bitboard f = froms; f;) {
        core::Square from = bb::pop_lsb(f);
        out.push_back(
            {from, st.enPassantSquare, core::PieceType::None, true, true, CastleSide::None});
      }
    }
  }
}

// ------------------------ Knights ------------------------

void MoveGenerator::genKnightMoves(const Board& board, core::Color side,
                                   std::vector<Move>& out) const {
  const bb::Bitboard knights = board.getPieces(side, core::PieceType::Knight);
  const bb::Bitboard own = board.getPieces(side);
  const bb::Bitboard enemyK = board.getPieces(~side, core::PieceType::King);
  const bb::Bitboard enemy = board.getPieces(~side) & ~enemyK;  // kein König

  for (bb::Bitboard n = knights; n;) {
    core::Square from = bb::pop_lsb(n);
    bb::Bitboard dest = bb::knight_attacks_from(from) & ~own & ~enemyK;
    while (dest) {
      core::Square to = bb::pop_lsb(dest);
      const bool isCap = (enemy & bb::sq_bb(to)) != 0;
      out.push_back({from, to, core::PieceType::None, isCap, false, CastleSide::None});
    }
  }
}

// ------------------------ Bishops ------------------------

void MoveGenerator::genBishopMoves(const Board& board, core::Color side,
                                   std::vector<Move>& out) const {
  const bb::Bitboard bishops = board.getPieces(side, core::PieceType::Bishop);
  const bb::Bitboard own = board.getPieces(side);
  const bb::Bitboard occ = board.getAllPieces();
  const bb::Bitboard enemyK = board.getPieces(~side, core::PieceType::King);
  const bb::Bitboard enemy = board.getPieces(~side) & ~enemyK;

  for (bb::Bitboard b = bishops; b;) {
    core::Square from = bb::pop_lsb(b);
    bb::Bitboard atk = magic::sliding_attacks(magic::Slider::Bishop, from, occ);
    bb::Bitboard dest = atk & ~own & ~enemyK;
    while (dest) {
      core::Square to = bb::pop_lsb(dest);
      const bool isCap = (enemy & bb::sq_bb(to)) != 0;
      out.push_back({from, to, core::PieceType::None, isCap, false, CastleSide::None});
    }
  }
}

// ------------------------ Rooks ------------------------

void MoveGenerator::genRookMoves(const Board& board, core::Color side,
                                 std::vector<Move>& out) const {
  const bb::Bitboard rooks = board.getPieces(side, core::PieceType::Rook);
  const bb::Bitboard own = board.getPieces(side);
  const bb::Bitboard occ = board.getAllPieces();
  const bb::Bitboard enemyK = board.getPieces(~side, core::PieceType::King);
  const bb::Bitboard enemy = board.getPieces(~side) & ~enemyK;

  for (bb::Bitboard r = rooks; r;) {
    core::Square from = bb::pop_lsb(r);
    bb::Bitboard atk = magic::sliding_attacks(magic::Slider::Rook, from, occ);
    bb::Bitboard dest = atk & ~own & ~enemyK;
    while (dest) {
      core::Square to = bb::pop_lsb(dest);
      const bool isCap = (enemy & bb::sq_bb(to)) != 0;
      out.push_back({from, to, core::PieceType::None, isCap, false, CastleSide::None});
    }
  }
}

// ------------------------ Queens ------------------------

void MoveGenerator::genQueenMoves(const Board& board, core::Color side,
                                  std::vector<Move>& out) const {
  const bb::Bitboard queens = board.getPieces(side, core::PieceType::Queen);
  const bb::Bitboard own = board.getPieces(side);
  const bb::Bitboard occ = board.getAllPieces();
  const bb::Bitboard enemyK = board.getPieces(~side, core::PieceType::King);
  const bb::Bitboard enemy = board.getPieces(~side) & ~enemyK;

  for (bb::Bitboard q = queens; q;) {
    core::Square from = bb::pop_lsb(q);
    bb::Bitboard atk = magic::sliding_attacks(magic::Slider::Bishop, from, occ) |
                       magic::sliding_attacks(magic::Slider::Rook, from, occ);
    bb::Bitboard dest = atk & ~own & ~enemyK;
    while (dest) {
      core::Square to = bb::pop_lsb(dest);
      const bool isCap = (enemy & bb::sq_bb(to)) != 0;
      out.push_back({from, to, core::PieceType::None, isCap, false, CastleSide::None});
    }
  }
}

// ------------------------ King (+ Castling) ------------------------

void MoveGenerator::genKingMoves(const Board& board, const GameState& st, core::Color side,
                                 std::vector<Move>& out) const {
  const bb::Bitboard king = board.getPieces(side, core::PieceType::King);
  if (!king) return;
  const core::Square from = static_cast<core::Square>(bb::ctz64(king));

  const bb::Bitboard own = board.getPieces(side);
  const bb::Bitboard enemyK = board.getPieces(~side, core::PieceType::King);
  const bb::Bitboard occ = board.getAllPieces();

  // normale Königszüge (kein Ziel = Gegnerkönig)
  bb::Bitboard dest = bb::king_attacks_from(from) & ~own & ~enemyK;
  while (dest) {
    core::Square to = bb::pop_lsb(dest);
    // isCap ist hier (falls du's brauchst) immer false, da enemyK maskiert wurde.
    out.push_back({from, to, core::PieceType::None, false, false, CastleSide::None});
  }

  auto attackedBy = [&](core::Square sq, core::Color by) -> bool {
    const bb::Bitboard occ = board.getAllPieces();
    const bb::Bitboard target = bb::sq_bb(sq);

    // Pawns: "von sq aus rückwärts"
    if (by == core::Color::White) {
      if ((bb::sw(target) | bb::se(target)) &
          board.getPieces(core::Color::White, core::PieceType::Pawn))
        return true;
    } else {
      if ((bb::nw(target) | bb::ne(target)) &
          board.getPieces(core::Color::Black, core::PieceType::Pawn))
        return true;
    }

    if (bb::knight_attacks_from(sq) & board.getPieces(by, core::PieceType::Knight)) return true;

    if (magic::sliding_attacks(magic::Slider::Bishop, sq, occ) &
        (board.getPieces(by, core::PieceType::Bishop) |
         board.getPieces(by, core::PieceType::Queen)))
      return true;

    if (magic::sliding_attacks(magic::Slider::Rook, sq, occ) &
        (board.getPieces(by, core::PieceType::Rook) | board.getPieces(by, core::PieceType::Queen)))
      return true;

    if (bb::king_attacks_from(sq) & board.getPieces(by, core::PieceType::King)) return true;

    return false;
  };

  const core::Color enemySide = ~side;

  // Castling (Pseudo-legal inkl. "nicht angegriffen"-Bedingung)
  if (side == core::Color::White) {
    // O-O (E1->G1): Felder F1,G1 frei; E1,F1,G1 nicht angegriffen
    if ((st.castlingRights & bb::Castling::WK) &&
        // optional: Turm muss auf H1 stehen
        (board.getPieces(core::Color::White, core::PieceType::Rook) & bb::sq_bb(bb::H1)) &&
        !(occ & (bb::sq_bb(core::Square{5}) | bb::sq_bb(core::Square{6})))) {
      if (!attackedBy(core::Square{4}, enemySide) && !attackedBy(core::Square{5}, enemySide) &&
          !attackedBy(core::Square{6}, enemySide)) {
        out.push_back(
            {bb::E1, core::Square{6}, core::PieceType::None, false, false, CastleSide::KingSide});
      }
    }
    // O-O-O (E1->C1): D1,C1,B1 frei; E1,D1,C1 nicht angegriffen
    if ((st.castlingRights & bb::Castling::WQ) &&
        (board.getPieces(core::Color::White, core::PieceType::Rook) & bb::sq_bb(bb::A1)) &&
        !(occ &
          (bb::sq_bb(core::Square{3}) | bb::sq_bb(core::Square{2}) | bb::sq_bb(core::Square{1})))) {
      if (!attackedBy(core::Square{4}, enemySide) && !attackedBy(core::Square{3}, enemySide) &&
          !attackedBy(core::Square{2}, enemySide)) {
        out.push_back(
            {bb::E1, core::Square{2}, core::PieceType::None, false, false, CastleSide::QueenSide});
      }
    }
  } else {
    // O-O (E8->G8): F8,G8 frei; E8,F8,G8 nicht angegriffen
    if ((st.castlingRights & bb::Castling::BK) &&
        (board.getPieces(core::Color::Black, core::PieceType::Rook) & bb::sq_bb(bb::H8)) &&
        !(occ & (bb::sq_bb(core::Square{61}) | bb::sq_bb(core::Square{62})))) {
      if (!attackedBy(core::Square{60}, enemySide) && !attackedBy(core::Square{61}, enemySide) &&
          !attackedBy(core::Square{62}, enemySide)) {
        out.push_back(
            {bb::E8, core::Square{62}, core::PieceType::None, false, false, CastleSide::KingSide});
      }
    }
    // O-O-O (E8->C8): D8,C8,B8 frei; E8,D8,C8 nicht angegriffen
    if ((st.castlingRights & bb::Castling::BQ) &&
        (board.getPieces(core::Color::Black, core::PieceType::Rook) & bb::sq_bb(bb::A8)) &&
        !(occ & (bb::sq_bb(core::Square{59}) | bb::sq_bb(core::Square{58}) |
                 bb::sq_bb(core::Square{57})))) {
      if (!attackedBy(core::Square{60}, enemySide) && !attackedBy(core::Square{59}, enemySide) &&
          !attackedBy(core::Square{58}, enemySide)) {
        out.push_back(
            {bb::E8, core::Square{58}, core::PieceType::None, false, false, CastleSide::QueenSide});
      }
    }
  }
}

}  // namespace lilia::model
