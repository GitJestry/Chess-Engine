#include "lilia/model/position.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <mutex>

#include "lilia/engine/config.hpp"
#include "lilia/model/core/magic.hpp"
#include "lilia/model/move_generator.hpp"

namespace lilia::model {

namespace {
// --------- Castling-Right Clear-Masken (FROM/TO) ----------
static std::once_flag s_cr_once;
static std::array<std::uint8_t, 64> CR_CLEAR_FROM{};
static std::array<std::uint8_t, 64> CR_CLEAR_TO{};

inline void init_cr_tables_once() {
  std::call_once(s_cr_once, [] {
    CR_CLEAR_FROM.fill(0);
    CR_CLEAR_TO.fill(0);

    // Könige bewegen / werden geschlagen
    CR_CLEAR_FROM[bb::E1] |= bb::Castling::WK | bb::Castling::WQ;
    CR_CLEAR_TO[bb::E1] |= bb::Castling::WK | bb::Castling::WQ;
    CR_CLEAR_FROM[bb::E8] |= bb::Castling::BK | bb::Castling::BQ;
    CR_CLEAR_TO[bb::E8] |= bb::Castling::BK | bb::Castling::BQ;

    // Türme bewegen / werden geschlagen
    CR_CLEAR_FROM[bb::H1] |= bb::Castling::WK;
    CR_CLEAR_TO[bb::H1] |= bb::Castling::WK;
    CR_CLEAR_FROM[bb::A1] |= bb::Castling::WQ;
    CR_CLEAR_TO[bb::A1] |= bb::Castling::WQ;
    CR_CLEAR_FROM[bb::H8] |= bb::Castling::BK;
    CR_CLEAR_TO[bb::H8] |= bb::Castling::BK;
    CR_CLEAR_FROM[bb::A8] |= bb::Castling::BQ;
    CR_CLEAR_TO[bb::A8] |= bb::Castling::BQ;
  });
}

// Schnelle Helper für SEE/Attack-Checks
inline bb::Bitboard pawn_attackers_to(core::Square sq, core::Color by, bb::Bitboard pawns) {
  const bb::Bitboard t = bb::sq_bb(sq);
  if (by == core::Color::White) {
    return (bb::sw(t) | bb::se(t)) & pawns;
  } else {
    return (bb::nw(t) | bb::ne(t)) & pawns;
  }
}

}  // namespace

// ---------------------- Utility Checks ----------------------

bool Position::checkInsufficientMaterial() {
  bb::Bitboard majorsMinors = 0;
  for (auto pt : {core::PieceType::Pawn, core::PieceType::Rook, core::PieceType::Queen}) {
    majorsMinors |= m_board.getPieces(core::Color::White, pt);
    majorsMinors |= m_board.getPieces(core::Color::Black, pt);
  }
  if (majorsMinors) return false;

  const bb::Bitboard whiteB = m_board.getPieces(core::Color::White, core::PieceType::Bishop);
  const bb::Bitboard blackB = m_board.getPieces(core::Color::Black, core::PieceType::Bishop);
  const bb::Bitboard whiteN = m_board.getPieces(core::Color::White, core::PieceType::Knight);
  const bb::Bitboard blackN = m_board.getPieces(core::Color::Black, core::PieceType::Knight);

  const int totalB = bb::popcount(whiteB) + bb::popcount(blackB);
  const int totalN = bb::popcount(whiteN) + bb::popcount(blackN);

  if (totalB == 0 && totalN == 0) return true;
  if ((totalB == 0 && totalN == 1) || (totalB == 1 && totalN == 0)) return true;

  if (totalB == 2 && totalN == 0) {
    auto same_color_squares = [](bb::Bitboard bishops) {
      int light = 0, dark = 0;
      while (bishops) {
        core::Square s = bb::pop_lsb(bishops);
        int f = (int)s & 7, r = (int)s >> 3;
        ((f + r) & 1) ? ++dark : ++light;
      }
      return std::max(light, dark);
    };
    if (same_color_squares(whiteB) == 2 || same_color_squares(blackB) == 2) return true;
  }

  if (totalB == 0 && totalN == 2) return true;

  return false;
}

bool Position::checkMoveRule() {
  return (m_state.halfmoveClock >= 100);
}

bool Position::checkRepetition() {
  int count = 0;
  const int n = (int)m_history.size();
  const int lim = std::min<int>(n, m_state.halfmoveClock);
  for (int back = 2; back <= lim; back += 2) {
    const int idx = n - back;
    if (idx < 0) break;
    if (m_history[idx].zobristKey == m_hash) {
      if (++count >= 2) return true;
    }
  }
  return false;
}

bool Position::inCheck() const {
  const bb::Bitboard kbb = m_board.getPieces(m_state.sideToMove, core::PieceType::King);
  if (!kbb) return false;
  const core::Square ksq = static_cast<core::Square>(bb::ctz64(kbb));
  return isSquareAttacked(ksq, ~m_state.sideToMove);
}

bool Position::isSquareAttacked(core::Square sq, core::Color by) const {
  const bb::Bitboard occ = m_board.getAllPieces();
  const bb::Bitboard pawns = m_board.getPieces(by, core::PieceType::Pawn);
  const bb::Bitboard knights = m_board.getPieces(by, core::PieceType::Knight);
  const bb::Bitboard bishops = m_board.getPieces(by, core::PieceType::Bishop);
  const bb::Bitboard rooks = m_board.getPieces(by, core::PieceType::Rook);
  const bb::Bitboard queens = m_board.getPieces(by, core::PieceType::Queen);
  const bb::Bitboard king = m_board.getPieces(by, core::PieceType::King);
  const bb::Bitboard target = bb::sq_bb(sq);

  if (pawn_attackers_to(sq, by, pawns)) return true;
  if (knights && (bb::knight_attacks_from(sq) & knights)) return true;
  if ((bishops | queens) &&
      (magic::sliding_attacks(magic::Slider::Bishop, sq, occ) & (bishops | queens)))
    return true;
  if ((rooks | queens) && (magic::sliding_attacks(magic::Slider::Rook, sq, occ) & (rooks | queens)))
    return true;
  if (king && (bb::king_attacks_from(sq) & king)) return true;

  (void)target;
  return false;
}

// ------- Static Exchange Evaluation (SEE), robust & einfach -------
bool Position::see(const model::Move& m) const {
  using core::Color;
  using core::PieceType;
  using core::Square;

  if (!m.isCapture && !m.isEnPassant) return true;

  auto ap = m_board.getPiece(m.from);
  if (!ap) return true;
  const Color us = ap->color;
  const Color them = static_cast<Color>(~us);
  const Square to = m.to;

  bb::Bitboard occ = m_board.getAllPieces();

  auto alive = [&](Color c, PieceType pt) -> bb::Bitboard {
    return m_board.getPieces(c, pt) & occ;
  };
  auto val = [&](PieceType pt) -> int { return engine::base_value[(int)pt]; };

  PieceType captured = PieceType::None;
  if (m.isEnPassant) {
    captured = PieceType::Pawn;
    const Square capSq = (us == Color::White) ? static_cast<Square>(static_cast<int>(to) - 8)
                                              : static_cast<Square>(static_cast<int>(to) + 8);
    occ &= ~bb::sq_bb(capSq);
  } else {
    if (auto cap = m_board.getPiece(to))
      captured = cap->type;
    else
      return true;
    occ &= ~bb::sq_bb(to);
  }

  occ &= ~bb::sq_bb(m.from);

  PieceType curOnTo = (m.promotion != PieceType::None) ? m.promotion : ap->type;
  occ |= bb::sq_bb(to);

  int gain[32];
  int d = 0;
  gain[d++] = val(captured);

  auto pawn_attackers = [&](Color c) -> bb::Bitboard {
    const bb::Bitboard tgt = bb::sq_bb(to);
    if (c == Color::White)
      return (bb::sw(tgt) | bb::se(tgt)) & alive(Color::White, PieceType::Pawn);
    return (bb::nw(tgt) | bb::ne(tgt)) & alive(Color::Black, PieceType::Pawn);
  };
  auto knight_attackers = [&](Color c) -> bb::Bitboard {
    const bb::Bitboard mask = bb::knight_attacks_from(to);
    return mask & alive(c, PieceType::Knight);
  };
  auto bishop_rays = [&](bb::Bitboard occNow) -> bb::Bitboard {
    return magic::sliding_attacks(magic::Slider::Bishop, to, occNow);
  };
  auto rook_rays = [&](bb::Bitboard occNow) -> bb::Bitboard {
    return magic::sliding_attacks(magic::Slider::Rook, to, occNow);
  };
  auto bishop_like = [&](Color c, bb::Bitboard occNow) -> bb::Bitboard {
    const bb::Bitboard rays = bishop_rays(occNow);
    return rays & (alive(c, PieceType::Bishop) | alive(c, PieceType::Queen));
  };
  auto rook_like = [&](Color c, bb::Bitboard occNow) -> bb::Bitboard {
    const bb::Bitboard rays = rook_rays(occNow);
    return rays & (alive(c, PieceType::Rook) | alive(c, PieceType::Queen));
  };
  auto king_attackers = [&](Color c) -> bb::Bitboard {
    const bb::Bitboard mask = bb::king_attacks_from(to);
    return mask & alive(c, PieceType::King);
  };

  auto pick_lva = [&](Color c, Square& fromSq, PieceType& pt) -> bool {
    bb::Bitboard bbx;
    if ((bbx = pawn_attackers(c))) {
      fromSq = bb::pop_lsb(bbx);
      pt = PieceType::Pawn;
      return true;
    }
    if ((bbx = knight_attackers(c))) {
      fromSq = bb::pop_lsb(bbx);
      pt = PieceType::Knight;
      return true;
    }
    if ((bbx = bishop_like(c, occ) & alive(c, PieceType::Bishop))) {
      fromSq = bb::pop_lsb(bbx);
      pt = PieceType::Bishop;
      return true;
    }
    if ((bbx = rook_like(c, occ) & alive(c, PieceType::Rook))) {
      fromSq = bb::pop_lsb(bbx);
      pt = PieceType::Rook;
      return true;
    }
    if ((bbx = ((bishop_like(c, occ) | rook_like(c, occ)) & alive(c, PieceType::Queen)))) {
      fromSq = bb::pop_lsb(bbx);
      pt = PieceType::Queen;
      return true;
    }
    if ((bbx = king_attackers(c))) {
      fromSq = bb::pop_lsb(bbx);
      pt = PieceType::King;
      return true;
    }
    return false;
  };

  Color side = them;

  while (true) {
    Square from2 = core::NO_SQUARE;
    PieceType pt2 = PieceType::None;
    if (!pick_lva(side, from2, pt2)) break;

    gain[d] = val(curOnTo) - gain[d - 1];
    ++d;

    if (gain[d - 1] < 0 && d > 1) break;

    occ &= ~bb::sq_bb(from2);

    curOnTo = pt2;
    side = static_cast<Color>(~side);

    if (d >= 31) break;
  }

  while (--d) gain[d - 1] = std::max(-gain[d], gain[d - 1]);
  return gain[0] >= 0;
}

bool Position::doMove(const Move& m) {
  if (m.from == m.to) return false;

  core::Color us = m_state.sideToMove;
  auto fromPiece = m_board.getPiece(m.from);
  if (!fromPiece || fromPiece->color != us) return false;

  // Promotions robust validieren
  if (m.promotion != core::PieceType::None) {
    if (fromPiece->type != core::PieceType::Pawn) return false;
    const int toRank = bb::rank_of(m.to);
    const bool onPromoRank = (us == core::Color::White) ? (toRank == 7) : (toRank == 0);
    if (!onPromoRank) return false;
    switch (m.promotion) {
      case core::PieceType::Knight:
      case core::PieceType::Bishop:
      case core::PieceType::Rook:
      case core::PieceType::Queen:
        break;
      default:
        return false;
    }
  }

  StateInfo st{};
  st.move = m;
  st.zobristKey = m_hash;
  st.prevCastlingRights = m_state.castlingRights;
  st.prevEnPassantSquare = m_state.enPassantSquare;
  st.prevHalfmoveClock = m_state.halfmoveClock;
  st.prevPawnKey = m_state.pawnKey;

  applyMove(m, st);

  // Illegal (eigener König im Schach) => rollback
  core::Color movedSide = ~m_state.sideToMove;
  bb::Bitboard kbbAfter = m_board.getPieces(movedSide, core::PieceType::King);
  if (!kbbAfter) {
    unapplyMove(st);
    m_hash = st.zobristKey;
    m_state.pawnKey = st.prevPawnKey;
    return false;
  }
  core::Square ksqAfter = static_cast<core::Square>(bb::ctz64(kbbAfter));
  if (isSquareAttacked(ksqAfter, m_state.sideToMove)) {
    unapplyMove(st);
    m_hash = st.zobristKey;
    m_state.pawnKey = st.prevPawnKey;
    return false;
  }

  m_history.push_back(st);
  return true;
}

void Position::undoMove() {
  if (m_history.empty()) return;
  StateInfo st = m_history.back();
  unapplyMove(st);
  m_hash = st.zobristKey;
  m_state.pawnKey = st.prevPawnKey;
  m_history.pop_back();
}

bool Position::doNullMove() {
  NullState st{};
  st.zobristKey = m_hash;
  st.prevCastlingRights = m_state.castlingRights;
  st.prevEnPassantSquare = m_state.enPassantSquare;
  st.prevHalfmoveClock = m_state.halfmoveClock;
  st.prevFullmoveNumber = m_state.fullmoveNumber;

  xorEPRelevant();
  m_state.enPassantSquare = core::NO_SQUARE;

  ++m_state.halfmoveClock;

  hashXorSide();
  m_state.sideToMove = ~m_state.sideToMove;
  if (m_state.sideToMove == core::Color::White) ++m_state.fullmoveNumber;

  m_null_history.push_back(st);
  return true;
}

void Position::undoNullMove() {
  if (m_null_history.empty()) return;
  NullState st = m_null_history.back();
  m_null_history.pop_back();

  m_state.sideToMove = ~m_state.sideToMove;
  hashXorSide();

  m_state.fullmoveNumber = st.prevFullmoveNumber;

  m_state.enPassantSquare = st.prevEnPassantSquare;
  xorEPRelevant();

  m_state.castlingRights = st.prevCastlingRights;
  m_state.halfmoveClock = st.prevHalfmoveClock;

  m_hash = st.zobristKey;
}

void Position::applyMove(const Move& m, StateInfo& st) {
  init_cr_tables_once();

  core::Color us = m_state.sideToMove;
  core::Color them = ~us;

  xorEPRelevant();
  const core::Square prevEP = m_state.enPassantSquare;
  m_state.enPassantSquare = core::NO_SQUARE;

  const auto fromPiece = m_board.getPiece(m.from);
  if (!fromPiece) return;
  const bool movingPawn = (fromPiece->type == core::PieceType::Pawn);

  // Rochade?
  bool isCastleMove = (m.castle != CastleSide::None);
  if (!isCastleMove && fromPiece->type == core::PieceType::King) {
    if (us == core::Color::White && m.from == bb::E1 &&
        (m.to == core::Square{6} || m.to == core::Square{2}))
      isCastleMove = true;
    if (us == core::Color::Black && m.from == bb::E8 &&
        (m.to == core::Square{62} || m.to == core::Square{58}))
      isCastleMove = true;
  }

  // En passant sicher erkennen
  bool isEP = m.isEnPassant;
  if (!isEP && movingPawn) {
    const int df = (int)m.to - (int)m.from;
    const bool diag = (df == 7 || df == 9 || df == -7 || df == -9);
    if (diag && prevEP != core::NO_SQUARE && m.to == prevEP) {
      if (!m_board.getPiece(m.to).has_value()) isEP = true;
    }
  }

  bool isCap = m.isCapture;
  if (!isCap && !isEP) {
    auto cap = m_board.getPiece(m.to);
    if (cap && cap->color == them) isCap = true;
  }

  // ------- [ACC] Capture aus Acc entfernen (vor Board-Mutation)
  if (isEP) {
    core::Square capSq = (us == core::Color::White) ? static_cast<core::Square>(m.to - 8)
                                                    : static_cast<core::Square>(m.to + 8);
    auto cap = m_board.getPiece(capSq);
    st.captured = cap.value_or(bb::Piece{core::PieceType::None, them});
    if (cap) {
      evalAcc_.remove_piece(them, core::PieceType::Pawn, (int)capSq);  // [ACC]
      hashXorPiece(them, core::PieceType::Pawn, capSq);
      m_board.removePiece(capSq);
    }
  } else if (isCap) {
    auto cap = m_board.getPiece(m.to);
    st.captured = cap.value_or(bb::Piece{core::PieceType::None, them});
    if (cap) {
      evalAcc_.remove_piece(them, st.captured.type, (int)m.to);  // [ACC]
      hashXorPiece(them, st.captured.type, m.to);
      m_board.removePiece(m.to);
    }
  } else {
    st.captured = bb::Piece{core::PieceType::None, them};
  }

  // ziehende Figur abtragen
  bb::Piece placed = *fromPiece;
  hashXorPiece(us, placed.type, m.from);
  m_board.removePiece(m.from);

  // Promotion anwenden
  if (m.promotion != core::PieceType::None) placed.type = m.promotion;

  // ------- [ACC] Mover updaten (Move / Promotion)
  if (m.promotion != core::PieceType::None) {
    evalAcc_.remove_piece(us, fromPiece->type, (int)m.from);  // [ACC]
    evalAcc_.add_piece(us, placed.type, (int)m.to);           // [ACC]
  } else {
    evalAcc_.move_piece(us, fromPiece->type, (int)m.from, (int)m.to);  // [ACC]
  }

  // Figur setzen
  hashXorPiece(us, placed.type, m.to);
  m_board.setPiece(m.to, placed);

  // Rochade: Turm versetzen
  if (isCastleMove) {
    if (us == core::Color::White) {
      if (m.to == static_cast<core::Square>(6) || m.castle == CastleSide::KingSide) {
        // H1 -> F1
        evalAcc_.move_piece(us, core::PieceType::Rook, (int)bb::H1, 5);  // [ACC]
        hashXorPiece(core::Color::White, core::PieceType::Rook, bb::H1);
        m_board.removePiece(bb::H1);
        hashXorPiece(core::Color::White, core::PieceType::Rook, static_cast<core::Square>(5));
        m_board.setPiece(static_cast<core::Square>(5),
                         bb::Piece{core::PieceType::Rook, core::Color::White});
      } else {
        // A1 -> D1
        evalAcc_.move_piece(us, core::PieceType::Rook, (int)bb::A1, 3);  // [ACC]
        hashXorPiece(core::Color::White, core::PieceType::Rook, bb::A1);
        m_board.removePiece(bb::A1);
        hashXorPiece(core::Color::White, core::PieceType::Rook, static_cast<core::Square>(3));
        m_board.setPiece(static_cast<core::Square>(3),
                         bb::Piece{core::PieceType::Rook, core::Color::White});
      }
    } else {
      if (m.to == static_cast<core::Square>(62) || m.castle == CastleSide::KingSide) {
        // H8 -> F8
        evalAcc_.move_piece(us, core::PieceType::Rook, (int)bb::H8, 61);  // [ACC]
        hashXorPiece(core::Color::Black, core::PieceType::Rook, bb::H8);
        m_board.removePiece(bb::H8);
        hashXorPiece(core::Color::Black, core::PieceType::Rook, static_cast<core::Square>(61));
        m_board.setPiece(static_cast<core::Square>(61),
                         bb::Piece{core::PieceType::Rook, core::Color::Black});
      } else {
        // A8 -> D8
        evalAcc_.move_piece(us, core::PieceType::Rook, (int)bb::A8, 59);  // [ACC]
        hashXorPiece(core::Color::Black, core::PieceType::Rook, bb::A8);
        m_board.removePiece(bb::A8);
        hashXorPiece(core::Color::Black, core::PieceType::Rook, static_cast<core::Square>(59));
        m_board.setPiece(static_cast<core::Square>(59),
                         bb::Piece{core::PieceType::Rook, core::Color::Black});
      }
    }
  }

  // 50-Züge-Regel
  if (placed.type == core::PieceType::Pawn || st.captured.type != core::PieceType::None)
    m_state.halfmoveClock = 0;
  else
    ++m_state.halfmoveClock;

  // neuen EP-Square (Doppelschritt)
  if (placed.type == core::PieceType::Pawn) {
    if (us == core::Color::White && bb::rank_of(m.from) == 1 && bb::rank_of(m.to) == 3)
      m_state.enPassantSquare = static_cast<core::Square>(m.from + 8);
    else if (us == core::Color::Black && bb::rank_of(m.from) == 6 && bb::rank_of(m.to) == 4)
      m_state.enPassantSquare = static_cast<core::Square>(m.from - 8);
  }

  // Castling-Rechte updaten (schnell über Tabellen)
  const std::uint8_t prevCR = m_state.castlingRights;
  m_state.castlingRights &= ~(CR_CLEAR_FROM[(int)m.from] | CR_CLEAR_TO[(int)m.to]);
  if (prevCR != m_state.castlingRights) hashSetCastling(prevCR, m_state.castlingRights);

  // sideToMove flippen
  hashXorSide();
  m_state.sideToMove = them;
  if (them == core::Color::White) ++m_state.fullmoveNumber;

  // neues EP ggf. in Hash
  xorEPRelevant();
}

void Position::unapplyMove(const StateInfo& st) {
  init_cr_tables_once();

  // Seite zurück
  m_state.sideToMove = ~m_state.sideToMove;
  hashXorSide();
  if (m_state.sideToMove == core::Color::Black) --m_state.fullmoveNumber;

  // Castling-Rechte zurück
  hashSetCastling(m_state.castlingRights, st.prevCastlingRights);
  m_state.castlingRights = st.prevCastlingRights;

  // EP zurück
  m_state.enPassantSquare = st.prevEnPassantSquare;
  xorEPRelevant();

  // 50-Züge
  m_state.halfmoveClock = st.prevHalfmoveClock;

  // Figuren zurück
  const Move& m = st.move;
  core::Color us = m_state.sideToMove;  // mover's side
  core::Color them = ~us;

  // Rochade rückgängig (Turm zurück)
  if (m.castle != CastleSide::None) {
    if (us == core::Color::White) {
      if (m.castle == CastleSide::KingSide) {
        // F1 -> H1
        evalAcc_.move_piece(us, core::PieceType::Rook, 5, (int)bb::H1);  // [ACC]
        hashXorPiece(core::Color::White, core::PieceType::Rook, static_cast<core::Square>(5));
        m_board.removePiece(static_cast<core::Square>(5));
        hashXorPiece(core::Color::White, core::PieceType::Rook, bb::H1);
        m_board.setPiece(bb::H1, bb::Piece{core::PieceType::Rook, core::Color::White});
      } else {
        // D1 -> A1
        evalAcc_.move_piece(us, core::PieceType::Rook, 3, (int)bb::A1);  // [ACC]
        hashXorPiece(core::Color::White, core::PieceType::Rook, static_cast<core::Square>(3));
        m_board.removePiece(static_cast<core::Square>(3));
        hashXorPiece(core::Color::White, core::PieceType::Rook, bb::A1);
        m_board.setPiece(bb::A1, bb::Piece{core::PieceType::Rook, core::Color::White});
      }
    } else {
      if (m.castle == CastleSide::KingSide) {
        // F8 -> H8
        evalAcc_.move_piece(us, core::PieceType::Rook, 61, (int)bb::H8);  // [ACC]
        hashXorPiece(core::Color::Black, core::PieceType::Rook, static_cast<core::Square>(61));
        m_board.removePiece(static_cast<core::Square>(61));
        hashXorPiece(core::Color::Black, core::PieceType::Rook, bb::H8);
        m_board.setPiece(bb::H8, bb::Piece{core::PieceType::Rook, core::Color::Black});
      } else {
        // D8 -> A8
        evalAcc_.move_piece(us, core::PieceType::Rook, 59, (int)bb::A8);  // [ACC]
        hashXorPiece(core::Color::Black, core::PieceType::Rook, static_cast<core::Square>(59));
        m_board.removePiece(static_cast<core::Square>(59));
        hashXorPiece(core::Color::Black, core::PieceType::Rook, bb::A8);
        m_board.setPiece(bb::A8, bb::Piece{core::PieceType::Rook, core::Color::Black});
      }
    }
  }

  // Zieher zurück auf from (Promo rückverwandeln)
  if (auto moving = m_board.getPiece(m.to)) {
    m_board.removePiece(m.to);
    bb::Piece placed = *moving;
    if (m.promotion != core::PieceType::None) placed.type = core::PieceType::Pawn;

    // ------- [ACC] Mover rückgängig
    if (m.promotion != core::PieceType::None) {
      evalAcc_.remove_piece(us, moving->type, (int)m.to);          // [ACC]
      evalAcc_.add_piece(us, core::PieceType::Pawn, (int)m.from);  // [ACC]
    } else {
      evalAcc_.move_piece(us, moving->type, (int)m.to, (int)m.from);  // [ACC]
    }

    hashXorPiece(us, moving->type, m.to);
    hashXorPiece(us, placed.type, m.from);
    m_board.setPiece(m.from, placed);
  } else {
    return;
  }

  // Capture zurück
  if (m.isEnPassant) {
    core::Square capSq = (us == core::Color::White) ? static_cast<core::Square>(m.to - 8)
                                                    : static_cast<core::Square>(m.to + 8);
    if (st.captured.type != core::PieceType::None) {
      evalAcc_.add_piece(them, st.captured.type, (int)capSq);  // [ACC]
      hashXorPiece(~us, st.captured.type, capSq);
      m_board.setPiece(capSq, st.captured);
    }
  } else if (st.captured.type != core::PieceType::None) {
    evalAcc_.add_piece(them, st.captured.type, (int)m.to);  // [ACC]
    hashXorPiece(~us, st.captured.type, m.to);
    m_board.setPiece(m.to, st.captured);
  }
}

}  // namespace lilia::model
