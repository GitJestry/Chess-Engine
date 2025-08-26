#include "lilia/model/move_generator.hpp"

#include "lilia/model/core/magic.hpp"

namespace lilia::model {

void MoveGenerator::generatePseudoLegalMoves(const Board& b, const GameState& st,
                                             std::vector<model::Move>& out) const {
  
  if (out.capacity() < 128) out.reserve(128);
  out.clear();

  core::Color side = st.sideToMove;

  
  genPawnMoves(b, st, side, out);
  genKnightMoves(b, side, out);
  genBishopMoves(b, side, out);
  genRookMoves(b, side, out);
  genQueenMoves(b, side, out);
  genKingMoves(b, st, side, out);
}

void MoveGenerator::genPawnMoves(const Board& board, const GameState& st, core::Color side,
                                 std::vector<Move>& out) const {
  bb::Bitboard us = board.pieces(side);
  bb::Bitboard them = board.pieces(~side);
  bb::Bitboard occ = board.allPieces();
  bb::Bitboard pawns = board.pieces(side, core::PieceType::Pawn);

  if (side == core::Color::White) {
    bb::Bitboard single = bb::north(pawns) & ~occ;
    bb::Bitboard promoPush = single & bb::RANK_8;
    bb::Bitboard quietPush = single & ~bb::RANK_8;

    bb::Bitboard dbl = bb::north(single) & ~occ & (bb::RANK_2 << 16);

    
    bb::Bitboard leftCap = (bb::nw(pawns) & them) & ~bb::RANK_8;
    bb::Bitboard rightCap = (bb::ne(pawns) & them) & ~bb::RANK_8;

    
    bb::Bitboard q = quietPush;
    while (q) {
      core::Square to = bb::pop_lsb(q);
      core::Square from = to - 8;
      out.push_back({from, to, core::PieceType::None, false, false, CastleSide::None});
    }
    
    bb::Bitboard d = dbl;
    while (d) {
      core::Square to = bb::pop_lsb(d);
      core::Square from = to - 16;
      out.push_back({from, to, core::PieceType::None, false, false, CastleSide::None});
    }
    
    bb::Bitboard lc = leftCap;
    while (lc) {
      core::Square to = bb::pop_lsb(lc);
      core::Square from = to - 7;
      out.push_back({from, to, core::PieceType::None, true, false, CastleSide::None});
    }
    bb::Bitboard rc = rightCap;
    while (rc) {
      core::Square to = bb::pop_lsb(rc);
      core::Square from = to - 9;
      out.push_back({from, to, core::PieceType::None, true, false, CastleSide::None});
    }
    
    bb::Bitboard pp = promoPush;
    while (pp) {
      core::Square to = bb::pop_lsb(pp);
      core::Square from = to - 8;
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight})
        out.push_back({from, to, pt, false, false, CastleSide::None});
    }
    
    bb::Bitboard lp = (bb::nw(pawns) & them) & bb::RANK_8;
    while (lp) {
      core::Square to = bb::pop_lsb(lp);
      core::Square from = to - 7;
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight})
        out.push_back({from, to, pt, true, false, CastleSide::None});
    }
    bb::Bitboard rp = (bb::ne(pawns) & them) & bb::RANK_8;
    while (rp) {
      core::Square to = bb::pop_lsb(rp);
      core::Square from = to - 9;
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight})
        out.push_back({from, to, pt, true, false, CastleSide::None});
    }

    
    if (st.enPassantSquare != core::NO_SQUARE) {
      core::Square ep = st.enPassantSquare;
      
      if (bb::file_of(ep) > 0) {
        core::Square from = static_cast<core::Square>(ep - 9);
        if (pawns & bb::sq_bb(from))
          out.push_back({from, ep, core::PieceType::None, true, true, CastleSide::None});
      }
      if (bb::file_of(ep) < 7) {
        core::Square from = static_cast<core::Square>(ep - 7);
        if (pawns & bb::sq_bb(from))
          out.push_back({from, ep, core::PieceType::None, true, true, CastleSide::None});
      }
    }
  } else {
    
    bb::Bitboard single = bb::south(pawns) & ~occ;
    bb::Bitboard promoPush = single & bb::RANK_1;
    bb::Bitboard quietPush = single & ~bb::RANK_1;

    bb::Bitboard dbl = bb::south(single) & ~occ & (bb::RANK_7 >> 16);

    
    bb::Bitboard leftCap = (bb::se(pawns) & them) & ~bb::RANK_1;
    bb::Bitboard rightCap = (bb::sw(pawns) & them) & ~bb::RANK_1;

    bb::Bitboard q = quietPush;
    while (q) {
      core::Square to = bb::pop_lsb(q);
      core::Square from = to + 8;
      out.push_back({from, to, core::PieceType::None, false, false, CastleSide::None});
    }
    bb::Bitboard d = dbl;
    while (d) {
      core::Square to = bb::pop_lsb(d);
      core::Square from = to + 16;
      out.push_back({from, to, core::PieceType::None, false, false, CastleSide::None});
    }
    bb::Bitboard lc = leftCap;
    while (lc) {
      core::Square to = bb::pop_lsb(lc);
      core::Square from = to + 7;
      out.push_back({from, to, core::PieceType::None, true, false, CastleSide::None});
    }
    bb::Bitboard rc = rightCap;
    while (rc) {
      core::Square to = bb::pop_lsb(rc);
      core::Square from = to + 9;
      out.push_back({from, to, core::PieceType::None, true, false, CastleSide::None});
    }

    
    bb::Bitboard pp = promoPush;
    while (pp) {
      core::Square to = bb::pop_lsb(pp);
      core::Square from = to + 8;
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight})
        out.push_back({from, to, pt, false, false, CastleSide::None});
    }
    
    bb::Bitboard lp = (bb::se(pawns) & them) & bb::RANK_1;
    while (lp) {
      core::Square to = bb::pop_lsb(lp);
      core::Square from = to + 7;
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight})
        out.push_back({from, to, pt, true, false, CastleSide::None});
    }
    bb::Bitboard rp = (bb::sw(pawns) & them) & bb::RANK_1;
    while (rp) {
      core::Square to = bb::pop_lsb(rp);
      core::Square from = to + 9;
      for (core::PieceType pt : {core::PieceType::Queen, core::PieceType::Rook,
                                 core::PieceType::Bishop, core::PieceType::Knight})
        out.push_back({from, to, pt, true, false, CastleSide::None});
    }

    if (st.enPassantSquare != core::NO_SQUARE) {
      bb::Bitboard ep_bb = bb::sq_bb(st.enPassantSquare);
      if (side == core::Color::White) {
        bb::Bitboard from_candidates = (bb::sw(ep_bb) | bb::se(ep_bb)) & pawns;
        while (from_candidates) {
          core::Square from = bb::pop_lsb(from_candidates);
          out.push_back(
              {from, st.enPassantSquare, core::PieceType::None, true, true, CastleSide::None});
        }
      } else {
        bb::Bitboard from_candidates = (bb::nw(ep_bb) | bb::ne(ep_bb)) & pawns;
        while (from_candidates) {
          core::Square from = bb::pop_lsb(from_candidates);
          out.push_back(
              {from, st.enPassantSquare, core::PieceType::None, true, true, CastleSide::None});
        }
      }
    }
  }
}

void MoveGenerator::genKnightMoves(const Board& board, core::Color side,
                                   std::vector<Move>& out) const {
  bb::Bitboard knights = board.pieces(side, core::PieceType::Knight);
  bb::Bitboard own = board.pieces(side);
  bb::Bitboard enemy = board.pieces(~side);

  bb::Bitboard n = knights;
  while (n) {
    core::Square from = bb::pop_lsb(n);
    bb::Bitboard atk = bb::knight_attacks_from(from);
    bb::Bitboard quiet = atk & ~own & ~enemy;
    bb::Bitboard caps = atk & enemy;

    bb::Bitboard q = quiet;
    while (q) {
      core::Square to = bb::pop_lsb(q);
      out.push_back({from, to, core::PieceType::None, false, false, CastleSide::None});
    }
    bb::Bitboard c = caps;
    while (c) {
      core::Square to = bb::pop_lsb(c);
      out.push_back({from, to, core::PieceType::None, true, false, CastleSide::None});
    }
  }
}

void MoveGenerator::genBishopMoves(const Board& board, core::Color side,
                                   std::vector<Move>& out) const {
  bb::Bitboard bishops = board.pieces(side, core::PieceType::Bishop);
  bb::Bitboard own = board.pieces(side);
  bb::Bitboard enemy = board.pieces(~side);
  bb::Bitboard occ = board.allPieces();

  bb::Bitboard b = bishops;
  while (b) {
    core::Square from = bb::pop_lsb(b);
    bb::Bitboard atk = magic::sliding_attacks(magic::Slider::Bishop, from, occ);
    bb::Bitboard quiet = atk & ~own & ~enemy;
    bb::Bitboard caps = atk & enemy;

    bb::Bitboard q = quiet;
    while (q) {
      core::Square to = bb::pop_lsb(q);
      out.push_back({from, to, core::PieceType::None, false, false, CastleSide::None});
    }
    bb::Bitboard c = caps;
    while (c) {
      core::Square to = bb::pop_lsb(c);
      out.push_back({from, to, core::PieceType::None, true, false, CastleSide::None});
    }
  }
}

void MoveGenerator::genRookMoves(const Board& board, core::Color side,
                                 std::vector<Move>& out) const {
  bb::Bitboard rooks = board.pieces(side, core::PieceType::Rook);
  bb::Bitboard own = board.pieces(side);
  bb::Bitboard enemy = board.pieces(~side);
  bb::Bitboard occ = board.allPieces();

  bb::Bitboard r = rooks;
  while (r) {
    core::Square from = bb::pop_lsb(r);
    bb::Bitboard atk = magic::sliding_attacks(magic::Slider::Rook, from, occ);
    bb::Bitboard quiet = atk & ~own & ~enemy;
    bb::Bitboard caps = atk & enemy;

    bb::Bitboard q = quiet;
    while (q) {
      core::Square to = bb::pop_lsb(q);
      out.push_back({from, to, core::PieceType::None, false, false, CastleSide::None});
    }
    bb::Bitboard c = caps;
    while (c) {
      core::Square to = bb::pop_lsb(c);
      out.push_back({from, to, core::PieceType::None, true, false, CastleSide::None});
    }
  }
}

void MoveGenerator::genQueenMoves(const Board& board, core::Color side,
                                  std::vector<Move>& out) const {
  bb::Bitboard queens = board.pieces(side, core::PieceType::Queen);
  bb::Bitboard own = board.pieces(side);
  bb::Bitboard enemy = board.pieces(~side);
  bb::Bitboard occ = board.allPieces();

  bb::Bitboard qcore = queens;
  while (qcore) {
    core::Square from = bb::pop_lsb(qcore);
    bb::Bitboard atk = magic::sliding_attacks(magic::Slider::Bishop, from, occ) |
                       magic::sliding_attacks(magic::Slider::Rook, from, occ);
    bb::Bitboard quiet = atk & ~own & ~enemy;
    bb::Bitboard caps = atk & enemy;

    bb::Bitboard q = quiet;
    while (q) {
      core::Square to = bb::pop_lsb(q);
      out.push_back({from, to, core::PieceType::None, false, false, CastleSide::None});
    }
    bb::Bitboard c = caps;
    while (c) {
      core::Square to = bb::pop_lsb(c);
      out.push_back({from, to, core::PieceType::None, true, false, CastleSide::None});
    }
  }
}

void MoveGenerator::genKingMoves(const Board& board, const GameState& st, core::Color side,
                                 std::vector<Move>& out) const {
  bb::Bitboard king = board.pieces(side, core::PieceType::King);
  if (!king) return;
  core::Square from = static_cast<core::Square>(bb::ctz64(king));

  bb::Bitboard own = board.pieces(side);
  bb::Bitboard enemy = board.pieces(~side);

  bb::Bitboard atk = bb::king_attacks_from(from);
  bb::Bitboard quiet = atk & ~own & ~enemy;
  bb::Bitboard caps = atk & enemy;

  bb::Bitboard q = quiet;
  while (q) {
    core::Square to = bb::pop_lsb(q);
    out.push_back({from, to, core::PieceType::None, false, false, CastleSide::None});
  }
  bb::Bitboard c = caps;
  while (c) {
    core::Square to = bb::pop_lsb(c);
    out.push_back({from, to, core::PieceType::None, true, false, CastleSide::None});
  }

  
  if (side == core::Color::White) {
    
    
    if ((st.castlingRights & bb::WK) &&
        !(board.allPieces() &
          (bb::sq_bb(static_cast<core::Square>(5)) | bb::sq_bb(static_cast<core::Square>(6))))) {
      out.push_back({bb::E1, static_cast<core::Square>(6), core::PieceType::None, false, false,
                     CastleSide::KingSide});
    }
    
    if ((st.castlingRights & bb::WQ) &&
        !(board.allPieces() &
          (bb::sq_bb(static_cast<core::Square>(3)) | bb::sq_bb(static_cast<core::Square>(2)) |
           bb::sq_bb(static_cast<core::Square>(1))))) {
      out.push_back({bb::E1, static_cast<core::Square>(2), core::PieceType::None, false, false,
                     CastleSide::QueenSide});
    }
  } else {
    
    if ((st.castlingRights & bb::BK) &&
        !(board.allPieces() &
          (bb::sq_bb(static_cast<core::Square>(61)) | bb::sq_bb(static_cast<core::Square>(62))))) {
      out.push_back({bb::E8, static_cast<core::Square>(62), core::PieceType::None, false, false,
                     CastleSide::KingSide});
    }
    if ((st.castlingRights & bb::BQ) &&
        !(board.allPieces() &
          (bb::sq_bb(static_cast<core::Square>(59)) | bb::sq_bb(static_cast<core::Square>(58)) |
           bb::sq_bb(static_cast<core::Square>(57))))) {
      out.push_back({bb::E8, static_cast<core::Square>(58), core::PieceType::None, false, false,
                     CastleSide::QueenSide});
    }
  }
}

}  
