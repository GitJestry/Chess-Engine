#include "lilia/model/position.hpp"

namespace lilia::model {

bool Position::isSquareAttacked(core::Square sq, core::Color by) const {
  bb::Bitboard occ = m_board.allPieces();

  // pawns
  if (by == core::Color::White) {
    bb::Bitboard pawns = m_board.pieces(core::Color::White, core::PieceType::Pawn);
    if (bb::white_pawn_attacks(pawns) & bb::sq_bb(sq)) return true;
  } else {
    bb::Bitboard pawns = m_board.pieces(core::Color::Black, core::PieceType::Pawn);
    if (bb::black_pawn_attacks(pawns) & bb::sq_bb(sq)) return true;
  }

  // knights
  bb::Bitboard knights = m_board.pieces(by, core::PieceType::Knight);
  for (bb::Bitboard n = knights; n;) {
    core::Square s = bb::pop_lsb(n);
    if (bb::knight_attacks_from(s) & bb::sq_bb(sq)) return true;
  }

  // bishops/queens
  bb::Bitboard bishops =
      m_board.pieces(by, core::PieceType::Bishop) | m_board.pieces(by, core::PieceType::Queen);
  for (bb::Bitboard b = bishops; b;) {
    core::Square s = bb::pop_lsb(b);
    if (bb::bishop_attacks(sq, occ) & bb::sq_bb(sq)) return true;
  }

  // rooks/queens
  bb::Bitboard rooks =
      m_board.pieces(by, core::PieceType::Rook) | m_board.pieces(by, core::PieceType::Queen);
  for (bb::Bitboard r = rooks; r;) {
    core::Square s = bb::pop_lsb(r);
    if (bb::rook_attacks(sq, occ) & bb::sq_bb(sq)) return true;
  }

  // king
  bb::Bitboard king = m_board.pieces(by, core::PieceType::King);
  if (king && (bb::king_attacks_from(static_cast<core::Square>(bb::ctz64(king))) & bb::sq_bb(sq)))
    return true;

  return false;
}

bool Position::doMove(const Move& m) {
  StateInfo st{};
  st.move = m;
  st.prevCastlingRights = m_state.castlingRights;
  st.prevEnPassantSquare = m_state.enPassantSquare;
  st.prevHalfmoveClock = m_state.halfmoveClock;

  // apply
  applyMove(m, st);

  // legality: after apply, sideToMove has flipped
  core::Color us = ~m_state.sideToMove;
  bb::Bitboard kbb = m_board.pieces(us, core::PieceType::King);
  core::Square ksq = static_cast<core::Square>(bb::ctz64(kbb));
  if (isSquareAttacked(ksq, m_state.sideToMove)) {
    unapplyMove(st);  // illegal
    return false;
  }

  m_history.push_back(st);
  return true;
}

void Position::undoMove() {
  if (m_history.empty()) return;
  StateInfo st = m_history.back();
  m_history.pop_back();
  unapplyMove(st);
}

// Helpers
void Position::applyMove(const Move& m, StateInfo& st) {
  core::Color us = m_state.sideToMove;
  core::Color them = ~us;

  // Zobrist: side to move flips at end; we’ll xor at the end (like Stockfish)
  // First, clear EP (if any) in hash/state (EP depends only on file)
  hashClearEP();
  m_state.enPassantSquare = 64;

  // Capture (incl. ep)
  if (m.isEnPassant) {
    core::Square capSq = (us == core::Color::White) ? static_cast<core::Square>(m.to - 8)
                                                    : static_cast<core::Square>(m.to + 8);
    auto cap = m_board.getPiece(capSq);
    st.captured = cap.value_or(bb::Piece{core::PieceType::None, them});
    if (cap) {
      // hash remove captured pawn
      hashXorPiece(them, core::PieceType::Pawn, capSq);
      m_board.removePiece(capSq);
    }
  } else if (m.isCapture) {
    auto cap = m_board.getPiece(m.to);
    st.captured = cap.value_or(bb::Piece{core::PieceType::None, them});
    if (cap) {
      hashXorPiece(them, st.captured.type, m.to);
      m_board.removePiece(m.to);
    }
  } else {
    st.captured = bb::Piece{core::PieceType::None, them};
  }

  // Move piece (and hash)
  auto moving = m_board.getPiece(m.from);
  bb::Piece placed = moving.value();  // must exist in valid position

  // hash: remove moving piece from 'from'
  hashXorPiece(us, placed.type, m.from);
  m_board.removePiece(m.from);

  // promotion?
  if (m.promotion != core::PieceType::None) placed.type = m.promotion;

  // hash: add piece to 'to'
  hashXorPiece(us, placed.type, m.to);
  m_board.setPiece(m.to, placed);

  // Castling rook hop (affects hash & board)
  if (m.castle != CastleSide::None) {
    if (us == core::Color::White) {
      if (m.castle == CastleSide::KingSide) {
        // H1 -> F1
        hashXorPiece(core::Color::White, core::PieceType::Rook, bb::H1);
        m_board.removePiece(bb::H1);
        hashXorPiece(core::Color::White, core::PieceType::Rook, static_cast<core::Square>(5));
        m_board.setPiece(static_cast<core::Square>(5),
                         bb::Piece{core::PieceType::Rook, core::Color::White});  // F1
      } else {
        // A1 -> D1
        hashXorPiece(core::Color::White, core::PieceType::Rook, bb::A1);
        m_board.removePiece(bb::A1);
        hashXorPiece(core::Color::White, core::PieceType::Rook, static_cast<core::Square>(3));
        m_board.setPiece(static_cast<core::Square>(3),
                         bb::Piece{core::PieceType::Rook, core::Color::White});  // D1
      }
    } else {
      if (m.castle == CastleSide::KingSide) {
        // H8 -> F8
        hashXorPiece(core::Color::Black, core::PieceType::Rook, bb::H8);
        m_board.removePiece(bb::H8);
        hashXorPiece(core::Color::Black, core::PieceType::Rook, static_cast<core::Square>(61));
        m_board.setPiece(static_cast<core::Square>(61),
                         bb::Piece{core::PieceType::Rook, core::Color::Black});  // F8
      } else {
        // A8 -> D8
        hashXorPiece(core::Color::Black, core::PieceType::Rook, bb::A8);
        m_board.removePiece(bb::A8);
        hashXorPiece(core::Color::Black, core::PieceType::Rook, static_cast<core::Square>(59));
        m_board.setPiece(static_cast<core::Square>(59),
                         bb::Piece{core::PieceType::Rook, core::Color::Black});  // D8
      }
    }
  }

  // Halfmove clock
  if (placed.type == core::PieceType::Pawn || st.captured.type != core::PieceType::None)
    m_state.halfmoveClock = 0;
  else
    ++m_state.halfmoveClock;

  // New en-passant core::square (only after double pawn push)
  if (placed.type == core::PieceType::Pawn) {
    if (us == core::Color::White && bb::rank_of(m.from) == 1 && bb::rank_of(m.to) == 3) {
      m_state.enPassantSquare = static_cast<core::Square>(m.from + 8);
    } else if (us == core::Color::Black && bb::rank_of(m.from) == 6 && bb::rank_of(m.to) == 4) {
      m_state.enPassantSquare = static_cast<core::Square>(m.from - 8);
    }
  }
  // Hash EP (file only) if set
  hashSetEP(m_state.enPassantSquare);

  // Castling rights (hash with prev/next)
  std::uint8_t prevCR = m_state.castlingRights;
  updateCastlingRightsOnMove(m.from, m.to);
  if (prevCR != m_state.castlingRights) hashSetCastling(prevCR, m_state.castlingRights);

  // Flip side in hash & state
  hashXorSide();
  m_state.sideToMove = them;
  if (them == core::Color::White) ++m_state.fullmoveNumber;
}

void Position::unapplyMove(const StateInfo& st) {
  // Side flips back
  m_state.sideToMove = ~m_state.sideToMove;
  hashXorSide();
  if (m_state.sideToMove == core::Color::Black) --m_state.fullmoveNumber;

  // revert castling rights
  hashSetCastling(m_state.castlingRights, st.prevCastlingRights);
  m_state.castlingRights = st.prevCastlingRights;

  // revert EP
  hashClearEP();
  m_state.enPassantSquare = st.prevEnPassantSquare;
  hashSetEP(m_state.enPassantSquare);

  m_state.halfmoveClock = st.prevHalfmoveClock;

  const Move& m = st.move;
  core::Color us = m_state.sideToMove;  // the side who made the move

  // Undo castling rook hop (board + hash)
  if (m.castle != CastleSide::None) {
    if (us == core::Color::White) {
      if (m.castle == CastleSide::KingSide) {
        // F1 -> H1
        hashXorPiece(core::Color::White, core::PieceType::Rook, static_cast<core::Square>(5));
        m_board.removePiece(static_cast<core::Square>(5));
        hashXorPiece(core::Color::White, core::PieceType::Rook, bb::H1);
        m_board.setPiece(bb::H1, bb::Piece{core::PieceType::Rook, core::Color::White});
      } else {
        // D1 -> A1
        hashXorPiece(core::Color::White, core::PieceType::Rook, static_cast<core::Square>(3));
        m_board.removePiece(static_cast<core::Square>(3));
        hashXorPiece(core::Color::White, core::PieceType::Rook, bb::A1);
        m_board.setPiece(bb::A1, bb::Piece{core::PieceType::Rook, core::Color::White});
      }
    } else {
      if (m.castle == CastleSide::KingSide) {
        // F8 -> H8
        hashXorPiece(core::Color::Black, core::PieceType::Rook, static_cast<core::Square>(61));
        m_board.removePiece(static_cast<core::Square>(61));
        hashXorPiece(core::Color::Black, core::PieceType::Rook, bb::H8);
        m_board.setPiece(bb::H8, bb::Piece{core::PieceType::Rook, core::Color::Black});
      } else {
        // D8 -> A8
        hashXorPiece(core::Color::Black, core::PieceType::Rook, static_cast<core::Square>(59));
        m_board.removePiece(static_cast<core::Square>(59));
        hashXorPiece(core::Color::Black, core::PieceType::Rook, bb::A8);
        m_board.setPiece(bb::A8, bb::Piece{core::PieceType::Rook, core::Color::Black});
      }
    }
  }

  // Move piece back (hash remove at 'to', add at 'from')
  auto moving = m_board.getPiece(m.to);
  if (moving) {
    hashXorPiece(us, moving->type, m.to);
    m_board.removePiece(m.to);
    bb::Piece placed = *moving;
    if (m.promotion != core::PieceType::None)
      placed.type = core::PieceType::Pawn;  // undo promotion
    hashXorPiece(us, placed.type, m.from);
    m_board.setPiece(m.from, placed);
  }

  // Restore captured
  if (m.isEnPassant) {
    core::Square capSq = (us == core::Color::White) ? static_cast<core::Square>(m.to - 8)
                                                    : static_cast<core::Square>(m.to + 8);
    if (st.captured.type != core::PieceType::None) {
      hashXorPiece(~us, st.captured.type, capSq);
      m_board.setPiece(capSq, st.captured);
    }
  } else if (st.captured.type != core::PieceType::None) {
    hashXorPiece(~us, st.captured.type, m.to);
    m_board.setPiece(m.to, st.captured);
  }
}

void Position::updateCastlingRightsOnMove(core::Square from, core::Square to) {
  auto clear = [&](std::uint8_t rights) { m_state.castlingRights &= ~rights; };

  // king moves/captured from E1/E8
  if (from == bb::E1 || to == bb::E1) {
    clear(bb::Castling::WK | bb::Castling::WQ);
  }
  if (from == bb::E8 || to == bb::E8) {
    clear(bb::Castling::BK | bb::Castling::BQ);
  }

  // rooks move/captured from corners
  if (from == bb::H1 || to == bb::H1) {
    clear(bb::Castling::WK);
  }
  if (from == bb::A1 || to == bb::A1) {
    clear(bb::Castling::WQ);
  }
  if (from == bb::H8 || to == bb::H8) {
    clear(bb::Castling::BK);
  }
  if (from == bb::A8 || to == bb::A8) {
    clear(bb::Castling::BQ);
  }
}

}  // namespace lilia::model
