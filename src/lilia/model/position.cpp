#include "lilia/model/position.hpp"

#include "lilia/engine/config.hpp"
#include "lilia/model/core/magic.hpp"
#include "lilia/model/move_generator.hpp"

namespace lilia::model {

bool Position::checkInsufficientMaterial() {
  // Wenn irgendein Bauer, Turm oder Dame auf dem Brett ist, keine Stellungs-Remis
  bb::Bitboard occ = 0;
  for (auto pt : {core::PieceType::Pawn, core::PieceType::Queen, core::PieceType::Rook}) {
    occ |= m_board.getPieces(core::Color::White, pt);
    occ |= m_board.getPieces(core::Color::Black, pt);
  }
  if (bb::popcount(occ) > 0) return false;

  bb::Bitboard whiteB = m_board.getPieces(core::Color::White, core::PieceType::Bishop);
  bb::Bitboard blackB = m_board.getPieces(core::Color::Black, core::PieceType::Bishop);
  bb::Bitboard whiteN = m_board.getPieces(core::Color::White, core::PieceType::Knight);
  bb::Bitboard blackN = m_board.getPieces(core::Color::Black, core::PieceType::Knight);

  const int wB = bb::popcount(whiteB), bB = bb::popcount(blackB);
  const int wN = bb::popcount(whiteN), bN = bb::popcount(blackN);

  const int totalB = wB + bB;
  const int totalN = wN + bN;

  // K vs K
  if (totalB == 0 && totalN == 0) return true;
  // K + N vs K
  if (totalB == 0 && totalN == 1) return true;
  // K + B vs K
  if (totalB == 1 && totalN == 0) return true;

  // K+B vs K+B (beide Seiten je 1 Läufer) und Läufer auf gleicher Feldfarbe
  if (wB == 1 && bB == 1 && totalN == 0) {
    const int wSq = bb::ctz64(whiteB);
    const int bSq = bb::ctz64(blackB);
    // gleiche Felderfarbe ⇒ theoretisch Remis
    if ((wSq & 1) == (bSq & 1)) return true;
  }

  // K+N vs K+N ist NICHT automatisch Remis (zwei Springer matt sind i. A. nicht erzwungen,
  // aber theoretisch möglich), viele Engines lassen das hier NICHT als Remis gelten.

  return false;
}

bool Position::inCheck() const {
  bb::Bitboard kbb = m_board.getPieces(m_state.sideToMove, core::PieceType::King);
  if (!kbb) return false;  // defensiv
  core::Square ksq = static_cast<core::Square>(bb::ctz64(kbb));
  return isSquareAttacked(ksq, ~m_state.sideToMove);
}

bool Position::checkMoveRule() {
  return (m_state.halfmoveClock >= 100);
}
bool Position::checkRepetition() {
  int count = 0;
  int limit = std::min<int>(m_history.size(), m_state.halfmoveClock);

  for (int i = 2; i <= limit; i += 2) {
    if (m_history[m_history.size() - i].zobristKey == m_hash) {
      if (++count >= 2) return true;
    }
  }
  return false;
}

bool Position::see(const model::Move& m) const {
  using bb::Bitboard;
  using core::Color;
  using core::PieceType;

  // Nur für Captures oder Promotions relevant (Quiescence)
  if (!m.isCapture && m.promotion == PieceType::None) return false;

  const Color side = m_board.getPiece(m.from).value().color;
  const Color enemy = ~side;

  // Arbeitskopien der Bitboards
  std::array<Bitboard, 6> wbbs{}, bbbs{};
  for (int pt = 0; pt < 6; ++pt) {
    wbbs[pt] = m_board.getPieces(Color::White, static_cast<PieceType>(pt));
    bbbs[pt] = m_board.getPieces(Color::Black, static_cast<PieceType>(pt));
  }
  Bitboard occ = m_board.getAllPieces();

  // Tauschfeld ist IMMER m.to (auch bei EP)
  const core::Square sq = m.to;

  // Erbeutete Figur + Wert
  int captured_value = 0;
  if (m.isEnPassant) {
    const core::Square capSq = (side == Color::White) ? static_cast<core::Square>(m.to - 8)
                                                      : static_cast<core::Square>(m.to + 8);
    // Entferne den geschlagenen Bauern vom Brettzustand
    if (side == Color::White) {
      // Weiß schlägt schwarz en passant ⇒ schwarzer Bauer fällt
      if (bbbs[static_cast<int>(PieceType::Pawn)] & bb::sq_bb(capSq))
        bbbs[static_cast<int>(PieceType::Pawn)] &= ~bb::sq_bb(capSq);
    } else {
      if (wbbs[static_cast<int>(PieceType::Pawn)] & bb::sq_bb(capSq))
        wbbs[static_cast<int>(PieceType::Pawn)] &= ~bb::sq_bb(capSq);
    }
    occ &= ~bb::sq_bb(capSq);
    captured_value = engine::base_value[static_cast<int>(PieceType::Pawn)];
  } else if (m.isCapture) {
    // Normales Capture: Figur auf m.to entfernen
    auto cap = m_board.getPiece(m.to);
    if (cap) {
      captured_value = engine::base_value[static_cast<int>(cap->type)];
      if (cap->color == Color::White)
        wbbs[static_cast<int>(cap->type)] &= ~bb::sq_bb(m.to);
      else
        bbbs[static_cast<int>(cap->type)] &= ~bb::sq_bb(m.to);
    }
    occ &= ~bb::sq_bb(m.to);
  }

  // Angreifer (inkl. Promotiontyp) von from nach sq setzen
  auto movingOpt = m_board.getPiece(m.from);
  if (!movingOpt) return false;  // defensive
  PieceType movingType = movingOpt->type;
  if (m.promotion != PieceType::None) movingType = m.promotion;

  if (side == Color::White) {
    wbbs[static_cast<int>(movingOpt->type)] &= ~bb::sq_bb(m.from);
    wbbs[static_cast<int>(movingType)] |= bb::sq_bb(sq);
  } else {
    bbbs[static_cast<int>(movingOpt->type)] &= ~bb::sq_bb(m.from);
    bbbs[static_cast<int>(movingType)] |= bb::sq_bb(sq);
  }
  occ &= ~bb::sq_bb(m.from);
  occ |= bb::sq_bb(sq);

  // Bewertungssequenz
  std::vector<int> gains;
  gains.reserve(32);
  gains.push_back(captured_value);

  auto piece_value = [&](PieceType pt) { return engine::base_value[static_cast<int>(pt)]; };

  // Hilfsfunktionen zum Finden des billigsten Angreifers auf 'sq'
  auto has_slider_attack = [&](magic::Slider sl, core::Square s, Bitboard occ_) -> bool {
    return (magic::sliding_attacks(sl, s, occ_) & bb::sq_bb(sq)) != 0;
  };
  auto find_lva = [&](Color who, Bitboard occ_local, std::array<Bitboard, 6>& W,
                      std::array<Bitboard, 6>& B) -> std::pair<int, PieceType> {
    // Reihenfolge: Bauer, Springer, Läufer, Turm, Dame, König
    const auto& mine = (who == Color::White) ? W : B;

    // Bauer
    Bitboard pawns = mine[static_cast<int>(PieceType::Pawn)];
    while (pawns) {
      int s = bb::ctz64(pawns);
      pawns &= pawns - 1;
      Bitboard atk = (who == Color::White)
                         ? bb::white_pawn_attacks(bb::sq_bb(static_cast<core::Square>(s)))
                         : bb::black_pawn_attacks(bb::sq_bb(static_cast<core::Square>(s)));
      if (atk & bb::sq_bb(sq)) return {s, PieceType::Pawn};
    }
    // Springer
    Bitboard knights = mine[static_cast<int>(PieceType::Knight)];
    while (knights) {
      int s = bb::ctz64(knights);
      knights &= knights - 1;
      if (bb::knight_attacks_from(static_cast<core::Square>(s)) & bb::sq_bb(sq))
        return {s, PieceType::Knight};
    }
    // Läufer
    Bitboard bishops = mine[static_cast<int>(PieceType::Bishop)];
    while (bishops) {
      int s = bb::ctz64(bishops);
      bishops &= bishops - 1;
      if (has_slider_attack(magic::Slider::Bishop, static_cast<core::Square>(s), occ_local))
        return {s, PieceType::Bishop};
    }
    // Turm
    Bitboard rooks = mine[static_cast<int>(PieceType::Rook)];
    while (rooks) {
      int s = bb::ctz64(rooks);
      rooks &= rooks - 1;
      if (has_slider_attack(magic::Slider::Rook, static_cast<core::Square>(s), occ_local))
        return {s, PieceType::Rook};
    }
    // Dame
    Bitboard queens = mine[static_cast<int>(PieceType::Queen)];
    while (queens) {
      int s = bb::ctz64(queens);
      queens &= queens - 1;
      if (has_slider_attack(magic::Slider::Rook, static_cast<core::Square>(s), occ_local) ||
          has_slider_attack(magic::Slider::Bishop, static_cast<core::Square>(s), occ_local))
        return {s, PieceType::Queen};
    }
    // König
    Bitboard king = mine[static_cast<int>(PieceType::King)];
    if (king) {
      int s = bb::ctz64(king);
      if (bb::king_attacks_from(static_cast<core::Square>(s)) & bb::sq_bb(sq))
        return {s, PieceType::King};
    }
    return {-1, PieceType::None};
  };

  // Jetzt abwechselnd „schlagen zurück“
  Color cur = enemy;
  Bitboard occ_local = occ;
  auto W = wbbs, B = bbbs;

  while (true) {
    auto [att_sq, att_type] = find_lva(cur, occ_local, W, B);
    if (att_sq < 0) break;

    // Angreifer vom Brett nehmen
    if (cur == Color::White)
      W[static_cast<int>(att_type)] &= ~bb::sq_bb(static_cast<core::Square>(att_sq));
    else
      B[static_cast<int>(att_type)] &= ~bb::sq_bb(static_cast<core::Square>(att_sq));

    occ_local &= ~bb::sq_bb(static_cast<core::Square>(att_sq));

    // Gewinnfolge aktualisieren
    gains.push_back(piece_value(att_type) - gains.back());

    // Seite wechseln
    cur = ~cur;
  }

  // Negamax-Backprop auf den Gewinnen
  for (int i = static_cast<int>(gains.size()) - 2; i >= 0; --i)
    gains[i] = std::max(-gains[i + 1], gains[i]);

  return !gains.empty() && gains[0] >= 0;
}

bool Position::isSquareAttacked(core::Square sq, core::Color by) const {
  bb::Bitboard occ = m_board.getAllPieces();

  if (by == core::Color::White) {
    bb::Bitboard pawns = m_board.getPieces(core::Color::White, core::PieceType::Pawn);
    if (bb::white_pawn_attacks(pawns) & bb::sq_bb(sq)) return true;
  } else {
    bb::Bitboard pawns = m_board.getPieces(core::Color::Black, core::PieceType::Pawn);
    if (bb::black_pawn_attacks(pawns) & bb::sq_bb(sq)) return true;
  }

  // knights
  bb::Bitboard knights = m_board.getPieces(by, core::PieceType::Knight);

  for (bb::Bitboard n = knights; n;) {
    core::Square s = bb::pop_lsb(n);
    if (bb::knight_attacks_from(s) & bb::sq_bb(sq)) return true;
  }

  bb::Bitboard bishops = m_board.getPieces(by, core::PieceType::Bishop) |
                         m_board.getPieces(by, core::PieceType::Queen);
  for (bb::Bitboard b = bishops; b;) {
    core::Square s = bb::pop_lsb(b);
    if (magic::sliding_attacks(magic::Slider::Bishop, s, occ) & bb::sq_bb(sq)) return true;
  }

  bb::Bitboard rooks =
      m_board.getPieces(by, core::PieceType::Rook) | m_board.getPieces(by, core::PieceType::Queen);
  for (bb::Bitboard r = rooks; r;) {
    core::Square s = bb::pop_lsb(r);

    if (magic::sliding_attacks(magic::Slider::Rook, s, occ) & bb::sq_bb(sq)) return true;
  }

  // king
  bb::Bitboard king = m_board.getPieces(by, core::PieceType::King);

  if (king && (bb::king_attacks_from(static_cast<core::Square>(bb::ctz64(king))) & bb::sq_bb(sq)))
    return true;

  return false;
}

bool Position::doMove(const Move& m) {
  core::Color us = m_state.sideToMove;
  core::Color them = ~us;

  if (m.castle != CastleSide::None) {
    // König-Position aktuell bestimmen
    bb::Bitboard kbb = m_board.getPieces(us, core::PieceType::King);
    if (!kbb) return false;  // kein König - inkonsistent

    core::Square ksq = static_cast<core::Square>(bb::ctz64(kbb));

    if (m.from != ksq) return false;

    if (isSquareAttacked(ksq, them)) return false;

    std::uint8_t cr = m_state.castlingRights;
    if (us == core::Color::White) {
      if (m.castle == CastleSide::KingSide && !(cr & bb::Castling::WK)) return false;
      if (m.castle == CastleSide::QueenSide && !(cr & bb::Castling::WQ)) return false;
    } else {
      if (m.castle == CastleSide::KingSide && !(cr & bb::Castling::BK)) return false;
      if (m.castle == CastleSide::QueenSide && !(cr & bb::Castling::BQ)) return false;
    }

    core::Square rookSq = static_cast<core::Square>(
        (us == core::Color::White) ? (m.castle == CastleSide::KingSide ? bb::H1 : bb::A1)
                                   : (m.castle == CastleSide::KingSide ? bb::H8 : bb::A8));
    auto rookPiece = m_board.getPiece(rookSq);
    if (!rookPiece) return false;
    if (rookPiece->type != core::PieceType::Rook || rookPiece->color != us) return false;

    auto squareNotEmpty = [&](core::Square s) { return m_board.getPiece(s).has_value(); };
    if (us == core::Color::White) {
      if (m.castle == CastleSide::KingSide) {
        if (squareNotEmpty(static_cast<core::Square>(5)) ||
            squareNotEmpty(static_cast<core::Square>(6)))
          return false;
      } else {
        if (squareNotEmpty(static_cast<core::Square>(3)) ||
            squareNotEmpty(static_cast<core::Square>(2)) ||
            squareNotEmpty(static_cast<core::Square>(1)))
          return false;
      }
    } else {
      if (m.castle == CastleSide::KingSide) {
        if (squareNotEmpty(static_cast<core::Square>(61)) ||
            squareNotEmpty(static_cast<core::Square>(62)))
          return false;
      } else {
        if (squareNotEmpty(static_cast<core::Square>(59)) ||
            squareNotEmpty(static_cast<core::Square>(58)) ||
            squareNotEmpty(static_cast<core::Square>(57)))
          return false;
      }
    }

    core::Square passSq1, passSq2;
    if (us == core::Color::White) {
      if (m.castle == CastleSide::KingSide) {
        passSq1 = static_cast<core::Square>(5);
        passSq2 = static_cast<core::Square>(6);
      } else {
        passSq1 = static_cast<core::Square>(3);
        passSq2 = static_cast<core::Square>(2);
      }
    } else {
      if (m.castle == CastleSide::KingSide) {
        passSq1 = static_cast<core::Square>(61);
        passSq2 = static_cast<core::Square>(62);
      } else {
        passSq1 = static_cast<core::Square>(59);
        passSq2 = static_cast<core::Square>(58);
      }
    }

    if (isSquareAttacked(passSq1, them) || isSquareAttacked(passSq2, them)) return false;
  }

  StateInfo st{};
  st.move = m;
  st.zobristKey = m_hash;
  st.prevCastlingRights = m_state.castlingRights;
  st.prevEnPassantSquare = m_state.enPassantSquare;
  st.prevHalfmoveClock = m_state.halfmoveClock;
  st.prevPawnKey = m_state.pawnKey;

  applyMove(m, st);

  core::Color us_after = ~m_state.sideToMove;
  bb::Bitboard kbb = m_board.getPieces(us_after, core::PieceType::King);
  core::Square ksq = static_cast<core::Square>(bb::ctz64(kbb));
  if (isSquareAttacked(ksq, m_state.sideToMove)) {
    unapplyMove(st);
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

  hashClearEP();
  m_state.enPassantSquare = static_cast<core::Square>(64);

  ++m_state.halfmoveClock;

  hashXorSide();
  m_state.sideToMove = ~m_state.sideToMove;
  if (m_state.sideToMove == core::Color::White) {
    ++m_state.fullmoveNumber;
  }

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

  hashClearEP();
  m_state.enPassantSquare = st.prevEnPassantSquare;
  hashSetEP(m_state.enPassantSquare);

  m_state.castlingRights = st.prevCastlingRights;

  m_state.halfmoveClock = st.prevHalfmoveClock;

  m_hash = st.zobristKey;
}

void Position::applyMove(const Move& m, StateInfo& st) {
  core::Color us = m_state.sideToMove;
  core::Color them = ~us;

  hashClearEP();
  m_state.enPassantSquare = core::NO_SQUARE;

  if (m.isEnPassant) {
    core::Square capSq = (us == core::Color::White) ? static_cast<core::Square>(m.to - 8)
                                                    : static_cast<core::Square>(m.to + 8);
    auto cap = m_board.getPiece(capSq);
    st.captured = cap.value_or(bb::Piece{core::PieceType::None, them});
    if (cap) {
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

  auto moving = m_board.getPiece(m.from);
  bb::Piece placed = moving.value();
  hashXorPiece(us, placed.type, m.from);

  m_board.removePiece(m.from);

  if (m.promotion != core::PieceType::None) placed.type = m.promotion;

  hashXorPiece(us, placed.type, m.to);
  m_board.setPiece(m.to, placed);

  if (m.castle != CastleSide::None) {
    if (us == core::Color::White) {
      if (m.castle == CastleSide::KingSide) {
        hashXorPiece(core::Color::White, core::PieceType::Rook, bb::H1);
        m_board.removePiece(bb::H1);
        hashXorPiece(core::Color::White, core::PieceType::Rook, static_cast<core::Square>(5));
        m_board.setPiece(static_cast<core::Square>(5),
                         bb::Piece{core::PieceType::Rook, core::Color::White});
      } else {
        hashXorPiece(core::Color::White, core::PieceType::Rook, bb::A1);
        m_board.removePiece(bb::A1);
        hashXorPiece(core::Color::White, core::PieceType::Rook, static_cast<core::Square>(3));
        m_board.setPiece(static_cast<core::Square>(3),
                         bb::Piece{core::PieceType::Rook, core::Color::White});
      }
    } else {
      if (m.castle == CastleSide::KingSide) {
        hashXorPiece(core::Color::Black, core::PieceType::Rook, bb::H8);
        m_board.removePiece(bb::H8);
        hashXorPiece(core::Color::Black, core::PieceType::Rook, static_cast<core::Square>(61));
        m_board.setPiece(static_cast<core::Square>(61),
                         bb::Piece{core::PieceType::Rook, core::Color::Black});
      } else {
        hashXorPiece(core::Color::Black, core::PieceType::Rook, bb::A8);
        m_board.removePiece(bb::A8);
        hashXorPiece(core::Color::Black, core::PieceType::Rook, static_cast<core::Square>(59));
        m_board.setPiece(static_cast<core::Square>(59),
                         bb::Piece{core::PieceType::Rook, core::Color::Black});
      }
    }
  }

  if (placed.type == core::PieceType::Pawn || st.captured.type != core::PieceType::None)
    m_state.halfmoveClock = 0;
  else
    ++m_state.halfmoveClock;

  if (placed.type == core::PieceType::Pawn) {
    if (us == core::Color::White && bb::rank_of(m.from) == 1 && bb::rank_of(m.to) == 3)
      m_state.enPassantSquare = static_cast<core::Square>(m.from + 8);
    else if (us == core::Color::Black && bb::rank_of(m.from) == 6 && bb::rank_of(m.to) == 4)
      m_state.enPassantSquare = static_cast<core::Square>(m.from - 8);
  }

  hashSetEP(m_state.enPassantSquare);

  std::uint8_t prevCR = m_state.castlingRights;
  updateCastlingRightsOnMove(m.from, m.to);
  if (prevCR != m_state.castlingRights) hashSetCastling(prevCR, m_state.castlingRights);

  hashXorSide();
  m_state.sideToMove = them;
  if (them == core::Color::White) ++m_state.fullmoveNumber;
}

void Position::unapplyMove(const StateInfo& st) {
  m_state.sideToMove = ~m_state.sideToMove;
  hashXorSide();
  if (m_state.sideToMove == core::Color::Black) --m_state.fullmoveNumber;

  hashSetCastling(m_state.castlingRights, st.prevCastlingRights);
  m_state.castlingRights = st.prevCastlingRights;

  hashClearEP();
  m_state.enPassantSquare = st.prevEnPassantSquare;
  hashSetEP(m_state.enPassantSquare);

  m_state.halfmoveClock = st.prevHalfmoveClock;

  const Move& m = st.move;
  core::Color us = m_state.sideToMove;

  if (m.castle != CastleSide::None) {
    if (us == core::Color::White) {
      if (m.castle == CastleSide::KingSide) {
        hashXorPiece(core::Color::White, core::PieceType::Rook, static_cast<core::Square>(5));
        m_board.removePiece(static_cast<core::Square>(5));
        hashXorPiece(core::Color::White, core::PieceType::Rook, bb::H1);
        m_board.setPiece(bb::H1, bb::Piece{core::PieceType::Rook, core::Color::White});
      } else {
        hashXorPiece(core::Color::White, core::PieceType::Rook, static_cast<core::Square>(3));
        m_board.removePiece(static_cast<core::Square>(3));
        hashXorPiece(core::Color::White, core::PieceType::Rook, bb::A1);
        m_board.setPiece(bb::A1, bb::Piece{core::PieceType::Rook, core::Color::White});
      }
    } else {
      if (m.castle == CastleSide::KingSide) {
        hashXorPiece(core::Color::Black, core::PieceType::Rook, static_cast<core::Square>(61));
        m_board.removePiece(static_cast<core::Square>(61));
        hashXorPiece(core::Color::Black, core::PieceType::Rook, bb::H8);
        m_board.setPiece(bb::H8, bb::Piece{core::PieceType::Rook, core::Color::Black});
      } else {
        hashXorPiece(core::Color::Black, core::PieceType::Rook, static_cast<core::Square>(59));
        m_board.removePiece(static_cast<core::Square>(59));
        hashXorPiece(core::Color::Black, core::PieceType::Rook, bb::A8);
        m_board.setPiece(bb::A8, bb::Piece{core::PieceType::Rook, core::Color::Black});
      }
    }
  }

  auto moving = m_board.getPiece(m.to);
  if (moving) {
    m_board.removePiece(m.to);
    bb::Piece placed = *moving;
    if (m.promotion != core::PieceType::None) placed.type = core::PieceType::Pawn;
    m_board.setPiece(m.from, placed);
  }

  if (m.isEnPassant) {
    core::Square capSq = (us == core::Color::White) ? static_cast<core::Square>(m.to - 8)
                                                    : static_cast<core::Square>(m.to + 8);
    if (st.captured.type != core::PieceType::None) {
      m_board.setPiece(capSq, st.captured);
    }
  } else if (st.captured.type != core::PieceType::None) {
    m_board.setPiece(m.to, st.captured);
  }
}

void Position::updateCastlingRightsOnMove(core::Square from, core::Square to) {
  auto clear = [&](std::uint8_t rights) { m_state.castlingRights &= ~rights; };

  if (from == bb::E1 || to == bb::E1) {
    clear(bb::Castling::WK | bb::Castling::WQ);
  }
  if (from == bb::E8 || to == bb::E8) {
    clear(bb::Castling::BK | bb::Castling::BQ);
  }

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
