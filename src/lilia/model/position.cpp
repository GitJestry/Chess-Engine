#include "lilia/model/position.hpp"

#include <algorithm>

#include "lilia/engine/config.hpp"
#include "lilia/model/core/magic.hpp"
#include "lilia/model/move_generator.hpp"

namespace lilia::model {

// ---------------------- Utility Checks ----------------------

bool Position::checkInsufficientMaterial() {
  // Wenn eine Seite noch Bauern/Türme/Damen hat -> nicht insufficient
  bb::Bitboard occ = 0;
  for (auto pt : {core::PieceType::Pawn, core::PieceType::Queen, core::PieceType::Rook}) {
    occ |= m_board.getPieces(core::Color::White, pt);
    occ |= m_board.getPieces(core::Color::Black, pt);
  }
  if (bb::popcount(occ) > 0) return false;

  // Zähle Leichtfiguren
  bb::Bitboard whiteB = m_board.getPieces(core::Color::White, core::PieceType::Bishop);
  bb::Bitboard blackB = m_board.getPieces(core::Color::Black, core::PieceType::Bishop);
  bb::Bitboard whiteN = m_board.getPieces(core::Color::White, core::PieceType::Knight);
  bb::Bitboard blackN = m_board.getPieces(core::Color::Black, core::PieceType::Knight);

  int totalB = bb::popcount(whiteB) + bb::popcount(blackB);
  int totalN = bb::popcount(whiteN) + bb::popcount(blackN);

  // König gegen König
  if (totalB == 0 && totalN == 0) return true;

  // König + einzelne Leichtfigur gegen König
  if (totalB == 0 && totalN == 1) return true;
  if (totalB == 1 && totalN == 0) return true;

  // Zwei Läufer auf gleicher Feldfarbe gegen König (egal ob beide bei einer Partei)
  if (totalB == 2 && totalN == 0) {
    // Prüfe Feldfarben-Parität beider Läufer
    auto same_color_squares = [&](bb::Bitboard bishops) -> int {
      int light = 0, dark = 0;
      bb::Bitboard b = bishops;
      while (b) {
        core::Square s = bb::pop_lsb(b);
        // Feldfarbe: (file + rank) % 2 == 0 -> hell (je nach Board-Definition)
        int f = (int)s & 7;
        int r = (int)s >> 3;
        ((f + r) & 1) ? ++dark : ++light;
      }
      // Wir geben die max Anz. gleicher Farbe zurück
      return std::max(light, dark);
    };
    int sameWhite = same_color_squares(whiteB);
    int sameBlack = same_color_squares(blackB);
    if (sameWhite == 2 || sameBlack == 2) return true;
  }

  // Zwei Springer gegen König gilt in Praxis meist als remis, aber theoretisch nicht immer.
  // Viele Engines zählen es als insufficient:
  if (totalB == 0 && totalN == 2) return true;

  return false;
}

bool Position::checkMoveRule() {
  return (m_state.halfmoveClock >= 100);
}

bool Position::checkRepetition() {
  int count = 0;
  int limit = std::min<int>((int)m_history.size(), m_state.halfmoveClock);
  for (int i = 2; i <= limit; i += 2) {
    const auto idx = (int)m_history.size() - i;
    if (idx >= 0 && m_history[idx].zobristKey == m_hash) {
      if (++count >= 2) return true;  // 3-fold
    }
  }
  return false;
}

bool Position::inCheck() const {
  bb::Bitboard kbb = m_board.getPieces(m_state.sideToMove, core::PieceType::King);
  if (!kbb) return false;
  core::Square ksq = static_cast<core::Square>(bb::ctz64(kbb));
  return isSquareAttacked(ksq, ~m_state.sideToMove);
}

bool Position::isSquareAttacked(core::Square sq, core::Color by) const {
  const bb::Bitboard occ = m_board.getAllPieces();
  const bb::Bitboard target = bb::sq_bb(sq);

  // Pawns (Achtung: „wer greift sq an?“ => gegengerichtete Angriffsmaske)
  if (by == core::Color::White) {
    if (bb::white_pawn_attacks(m_board.getPieces(core::Color::White, core::PieceType::Pawn)) &
        target)
      return true;
  } else {
    if (bb::black_pawn_attacks(m_board.getPieces(core::Color::Black, core::PieceType::Pawn)) &
        target)
      return true;
  }

  // Knights
  bb::Bitboard n = m_board.getPieces(by, core::PieceType::Knight);
  if (n && (bb::knight_attacks_from(sq) & n)) return true;

  // Bishops/Queens (diagonal)
  {
    const bb::Bitboard diagAttackers = m_board.getPieces(by, core::PieceType::Bishop) |
                                       m_board.getPieces(by, core::PieceType::Queen);
    if (diagAttackers && (magic::sliding_attacks(magic::Slider::Bishop, sq, occ) & diagAttackers))
      return true;
  }

  // Rooks/Queens (orthogonal)
  {
    const bb::Bitboard orthoAttackers = m_board.getPieces(by, core::PieceType::Rook) |
                                        m_board.getPieces(by, core::PieceType::Queen);
    if (orthoAttackers && (magic::sliding_attacks(magic::Slider::Rook, sq, occ) & orthoAttackers))
      return true;
  }

  // King
  bb::Bitboard k = m_board.getPieces(by, core::PieceType::King);
  if (k && (bb::king_attacks_from(sq) & k)) return true;

  return false;
}
bool Position::see(const model::Move& m) const {
  using core::Color;
  using core::PieceType;

  // Nur für Captures/EP relevant – sonst neutral (nicht negativ)
  if (!m.isCapture && !m.isEnPassant) return true;

  // Lokale Kopien (veränderbar)
  bb::Bitboard occ = m_board.getAllPieces();
  std::array<bb::Bitboard, 6> wbbs{}, bbbs{};
  for (int pt = 0; pt < 6; ++pt) {
    wbbs[pt] = m_board.getPieces(Color::White, static_cast<PieceType>(pt));
    bbbs[pt] = m_board.getPieces(Color::Black, static_cast<PieceType>(pt));
  }

  // Angreifer
  auto attackerP = m_board.getPiece(m.from);
  if (!attackerP) return true;  // defensiv: lieber nicht zu streng
  const Color us = attackerP->color;
  const Color them = ~us;

  const core::Square sq = m.to;

  // Opfer korrekt entfernen + captured value
  int capturedVal = 0;
  if (m.isEnPassant) {
    // EP: Das Opfer steht "hinter" dem Zielfeld
    const core::Square capSq = (us == Color::White) ? static_cast<core::Square>(sq - 8)
                                                    : static_cast<core::Square>(sq + 8);
    capturedVal = engine::base_value[(int)PieceType::Pawn];
    occ &= ~bb::sq_bb(capSq);
    if (us == Color::White)
      bbbs[(int)PieceType::Pawn] &= ~bb::sq_bb(capSq);
    else
      wbbs[(int)PieceType::Pawn] &= ~bb::sq_bb(capSq);
  } else {
    if (auto cap = m_board.getPiece(sq)) {
      capturedVal = engine::base_value[(int)cap->type];
      occ &= ~bb::sq_bb(sq);
      if (cap->color == Color::White)
        wbbs[(int)cap->type] &= ~bb::sq_bb(sq);
      else
        bbbs[(int)cap->type] &= ~bb::sq_bb(sq);
    } else {
      // "Capture" auf leeres Feld – behandle neutral (nicht negativ)
      return true;
    }
  }

  // Angreifer vom Ursprungsfeld entfernen
  occ &= ~bb::sq_bb(m.from);
  if (us == Color::White)
    wbbs[(int)attackerP->type] &= ~bb::sq_bb(m.from);
  else
    bbbs[(int)attackerP->type] &= ~bb::sq_bb(m.from);

  // Ziel gilt als belegt (mit ziehender Figur; Promotions berücksichtigen)
  PieceType curOccType = (m.promotion != PieceType::None) ? m.promotion : attackerP->type;
  Color curOccSide = us;
  occ |= bb::sq_bb(sq);
  if (curOccSide == Color::White)
    wbbs[(int)curOccType] |= bb::sq_bb(sq);
  else
    bbbs[(int)curOccType] |= bb::sq_bb(sq);

  // Attack-Generator lokal (nur für EVAL der Folge-Schläge)
  auto attacks_from_local = [&](Color who, PieceType pt, core::Square s,
                                bb::Bitboard o) -> bb::Bitboard {
    switch (pt) {
      case PieceType::Pawn:
        return (who == Color::White) ? bb::white_pawn_attacks(bb::sq_bb(s))
                                     : bb::black_pawn_attacks(bb::sq_bb(s));
      case PieceType::Knight:
        return bb::knight_attacks_from(s);
      case PieceType::Bishop:
        return magic::sliding_attacks(magic::Slider::Bishop, s, o);
      case PieceType::Rook:
        return magic::sliding_attacks(magic::Slider::Rook, s, o);
      case PieceType::Queen:
        return magic::sliding_attacks(magic::Slider::Bishop, s, o) |
               magic::sliding_attacks(magic::Slider::Rook, s, o);
      case PieceType::King:
        return bb::king_attacks_from(s);
      default:
        return 0;
    }
  };

  // Least Valuable Attacker finden (unter aktueller Belegung)
  auto find_lva = [&](Color who, core::Square target, bb::Bitboard o) -> std::pair<int, PieceType> {
    static constexpr PieceType order[6] = {PieceType::Pawn, PieceType::Knight, PieceType::Bishop,
                                           PieceType::Rook, PieceType::Queen,  PieceType::King};
    for (PieceType pt : order) {
      bb::Bitboard set = (who == Color::White) ? wbbs[(int)pt] : bbbs[(int)pt];
      bb::Bitboard scan = set;
      while (scan) {
        core::Square s = bb::pop_lsb(scan);
        if (attacks_from_local(who, pt, s, o) & bb::sq_bb(target)) return {(int)s, pt};
      }
    }
    return {-1, PieceType::Pawn};
  };

  // Standard-Gain-Array
  int gain[64];
  int d = 0;
  gain[d++] = capturedVal;

  Color side = them;

  // Folge-Schlagkette
  while (true) {
    auto [aSq, aType] = find_lva(side, sq, occ);
    if (aSq < 0) break;

    // bisherigen Besetzer vom Ziel entfernen
    if (curOccSide == Color::White)
      wbbs[(int)curOccType] &= ~bb::sq_bb(sq);
    else
      bbbs[(int)curOccType] &= ~bb::sq_bb(sq);

    // Angreifer vom Ursprungsfeld entfernen
    if (side == Color::White)
      wbbs[(int)aType] &= ~bb::sq_bb(static_cast<core::Square>(aSq));
    else
      bbbs[(int)aType] &= ~bb::sq_bb(static_cast<core::Square>(aSq));
    occ &= ~bb::sq_bb(static_cast<core::Square>(aSq));

    // Wert des gerade ziehenden Angreifers abziehen/aufsummieren
    gain[d] = engine::base_value[(int)aType] - gain[d - 1];
    ++d;

    // Neuer Besetzer ist der eben ziehende Angreifer
    curOccSide = side;
    curOccType = aType;

    // Ziel wieder als belegt markieren + in die Seiten-BBs eintragen
    occ |= bb::sq_bb(sq);
    if (curOccSide == Color::White)
      wbbs[(int)curOccType] |= bb::sq_bb(sq);
    else
      bbbs[(int)curOccType] |= bb::sq_bb(sq);

    side = ~side;
    if (d >= 63) break;  // Sicherheitsbremse
  }

  // Rückwärts-Maximierung
  while (--d) gain[d - 1] = std::max(-gain[d], gain[d - 1]);

  return gain[0] >= 0;
}

bool Position::doMove(const Move& m) {
  if (m.from == m.to) return false;

  core::Color us = m_state.sideToMove;
  core::Color them = ~us;

  auto fromPiece = m_board.getPiece(m.from);
  if (!fromPiece) return false;              // kein Stück auf from
  if (fromPiece->color != us) return false;  // falsche Seite

  // --- NEU: Promotions robust validieren ---
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
        break;  // erlaubt
      default:
        return false;  // König/None etc. verbieten
    }
  }

  StateInfo st{};
  st.move = m;
  st.zobristKey = m_hash;
  st.prevCastlingRights = m_state.castlingRights;
  st.prevEnPassantSquare = m_state.enPassantSquare;
  st.prevHalfmoveClock = m_state.halfmoveClock;
  st.prevPawnKey = m_state.pawnKey;  // <<-- WICHTIG: PawnKey für Undo sichern

  applyMove(m, st);

  // Illegal (eigener König im Schach)? -> rückgängig
  core::Color movedSide = ~m_state.sideToMove;
  bb::Bitboard kbbAfter = m_board.getPieces(movedSide, core::PieceType::King);
  if (!kbbAfter) {
    unapplyMove(st);
    m_hash = st.zobristKey;
    m_state.pawnKey = st.prevPawnKey;  // <<-- PawnKey exakt wiederherstellen
    return false;
  }
  core::Square ksqAfter = static_cast<core::Square>(bb::ctz64(kbbAfter));
  if (isSquareAttacked(ksqAfter, m_state.sideToMove)) {
    unapplyMove(st);
    m_hash = st.zobristKey;
    m_state.pawnKey = st.prevPawnKey;  // <<-- PawnKey exakt wiederherstellen
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

  // altes EP (falls relevant) aus dem Hash
  xorEPRelevant();
  m_state.enPassantSquare = core::NO_SQUARE;

  ++m_state.halfmoveClock;

  // Seite flippen
  hashXorSide();
  m_state.sideToMove = ~m_state.sideToMove;
  if (m_state.sideToMove == core::Color::White) ++m_state.fullmoveNumber;

  // neues EP gibt es im Nullmove nicht
  m_null_history.push_back(st);
  return true;
}

void Position::undoNullMove() {
  if (m_null_history.empty()) return;

  NullState st = m_null_history.back();
  m_null_history.pop_back();

  // Seite zurückflippen
  m_state.sideToMove = ~m_state.sideToMove;
  hashXorSide();

  m_state.fullmoveNumber = st.prevFullmoveNumber;

  // EP wieder in Hash aufnehmen (falls relevant)
  m_state.enPassantSquare = st.prevEnPassantSquare;
  xorEPRelevant();

  m_state.castlingRights = st.prevCastlingRights;
  m_state.halfmoveClock = st.prevHalfmoveClock;

  m_hash = st.zobristKey;
}

void Position::applyMove(const Move& m, StateInfo& st) {
  core::Color us = m_state.sideToMove;
  core::Color them = ~us;

  // EP aus aktuellem Hash entfernen (falls relevant)
  xorEPRelevant();

  // >>> NEU: alten EP-Square cachen, BEVOR wir ihn löschen
  const core::Square prevEP = m_state.enPassantSquare;

  // jetzt EP resetten (wir setzen ggf. später neuen EP-Square)
  m_state.enPassantSquare = core::NO_SQUARE;

  const auto fromPiece = m_board.getPiece(m.from);
  if (!fromPiece) return;
  const bool movingPawn = (fromPiece->type == core::PieceType::Pawn);

  // Rochade-Erkennung wie gehabt (m.castle oder „König 2 Felder“)
  bool isCastleMove = (m.castle != CastleSide::None);
  if (!isCastleMove && fromPiece->type == core::PieceType::King) {
    if (us == core::Color::White && m.from == bb::E1 &&
        (m.to == core::Square{6} || m.to == core::Square{2}))
      isCastleMove = true;
    if (us == core::Color::Black && m.from == bb::E8 &&
        (m.to == core::Square{62} || m.to == core::Square{58}))
      isCastleMove = true;
  }

  // --- EP-Erkennung ROBUST ---
  bool isEP = m.isEnPassant;
  if (!isEP && movingPawn) {
    const int df = (int)m.to - (int)m.from;
    const bool diag = (df == 7 || df == 9 || df == -7 || df == -9);
    if (diag && prevEP != core::NO_SQUARE && m.to == prevEP) {
      // Ziel leer? Dann ist es wirklich EP
      if (!m_board.getPiece(m.to).has_value()) isEP = true;
    }
  }

  // --- „normales“ Capture?
  bool isCap = m.isCapture;
  if (!isCap && !isEP) {
    auto cap = m_board.getPiece(m.to);
    if (cap && cap->color == them) isCap = true;
  }

  // --- Capture anwenden (EP/Capture robust) ---
  if (isEP) {
    // EP: der geschlagene Bauer steht hinter dem Zielfeld
    core::Square capSq = (us == core::Color::White) ? static_cast<core::Square>(m.to - 8)
                                                    : static_cast<core::Square>(m.to + 8);
    auto cap = m_board.getPiece(capSq);
    st.captured = cap.value_or(bb::Piece{core::PieceType::None, them});
    if (cap) {
      // Hash & PawnKey: Bauer des Gegners entfernen
      hashXorPiece(them, core::PieceType::Pawn, capSq);  // <<< pawnKey aktualisiert sich hier
      m_board.removePiece(capSq);
    }
  } else if (isCap) {
    auto cap = m_board.getPiece(m.to);
    st.captured = cap.value_or(bb::Piece{core::PieceType::None, them});
    if (cap) {
      // Hash & PawnKey: gegnerisches Opfer entfernen (falls Bauer -> pawnKey flip)
      hashXorPiece(them, st.captured.type, m.to);
      m_board.removePiece(m.to);
    }
  } else {
    st.captured = bb::Piece{core::PieceType::None, them};
  }

  // --- ziehende Figur vom from-Feld entfernen (Hash & pawnKey, falls Bauer) ---
  bb::Piece placed = *fromPiece;
  hashXorPiece(us, placed.type, m.from);  // <<< bei Bauer: pawnKey flip
  m_board.removePiece(m.from);

  // --- Promotion anwenden (Bauer wird zu N/B/R/Q) ---
  if (m.promotion != core::PieceType::None) {
    placed.type = m.promotion;  // kein Bauer mehr -> kein pawnKey an 'to'
  }

  // --- Figur auf 'to' setzen (Hash & pawnKey, falls es nach der Promo noch ein Bauer wäre) ---
  hashXorPiece(us, placed.type, m.to);
  m_board.setPiece(m.to, placed);

  // --- Rochade: Turm versetzen (Hash & pawnKey korrekt via hashXorPiece) ---
  if (isCastleMove) {
    if (us == core::Color::White) {
      if (m.to == static_cast<core::Square>(6) /*G1*/ || m.castle == CastleSide::KingSide) {
        hashXorPiece(core::Color::White, core::PieceType::Rook, bb::H1);
        m_board.removePiece(bb::H1);
        hashXorPiece(core::Color::White, core::PieceType::Rook,
                     static_cast<core::Square>(5));  // F1
        m_board.setPiece(static_cast<core::Square>(5),
                         bb::Piece{core::PieceType::Rook, core::Color::White});
      } else {
        hashXorPiece(core::Color::White, core::PieceType::Rook, bb::A1);
        m_board.removePiece(bb::A1);
        hashXorPiece(core::Color::White, core::PieceType::Rook,
                     static_cast<core::Square>(3));  // C1->D1 rook lands on D1 (=3)
        m_board.setPiece(static_cast<core::Square>(3),
                         bb::Piece{core::PieceType::Rook, core::Color::White});
      }
    } else {
      if (m.to == static_cast<core::Square>(62) /*G8*/ || m.castle == CastleSide::KingSide) {
        hashXorPiece(core::Color::Black, core::PieceType::Rook, bb::H8);
        m_board.removePiece(bb::H8);
        hashXorPiece(core::Color::Black, core::PieceType::Rook,
                     static_cast<core::Square>(61));  // F8
        m_board.setPiece(static_cast<core::Square>(61),
                         bb::Piece{core::PieceType::Rook, core::Color::Black});
      } else {
        hashXorPiece(core::Color::Black, core::PieceType::Rook, bb::A8);
        m_board.removePiece(bb::A8);
        hashXorPiece(core::Color::Black, core::PieceType::Rook,
                     static_cast<core::Square>(59));  // D8
        m_board.setPiece(static_cast<core::Square>(59),
                         bb::Piece{core::PieceType::Rook, core::Color::Black});
      }
    }
  }

  // --- 50-Züge-Regel (Bauer bewegt ODER capture) ---
  if (placed.type == core::PieceType::Pawn || st.captured.type != core::PieceType::None)
    m_state.halfmoveClock = 0;
  else
    ++m_state.halfmoveClock;

  // --- neuen EP-Square setzen (nur bei Doppelzug eines Bauern) ---
  if (placed.type == core::PieceType::Pawn) {
    if (us == core::Color::White && bb::rank_of(m.from) == 1 && bb::rank_of(m.to) == 3)
      m_state.enPassantSquare = static_cast<core::Square>(m.from + 8);
    else if (us == core::Color::Black && bb::rank_of(m.from) == 6 && bb::rank_of(m.to) == 4)
      m_state.enPassantSquare = static_cast<core::Square>(m.from - 8);
    // (kein direkter Einfluss auf pawnKey – nur Zobrist-Hash via epHashIfRelevant)
  }

  // --- Castling-Rechte updaten (Hash für CR via hashSetCastling) ---
  const std::uint8_t prevCR = m_state.castlingRights;
  updateCastlingRightsOnMove(m.from, m.to);
  if (prevCR != m_state.castlingRights) hashSetCastling(prevCR, m_state.castlingRights);

  // --- sideToMove flippen (Hash side xor) ---
  hashXorSide();
  m_state.sideToMove = them;
  if (them == core::Color::White) ++m_state.fullmoveNumber;

  // --- neues EP (falls gesetzt) wieder in den Hash (mit neuer sideToMove bewertet) ---
  xorEPRelevant();
}

void Position::unapplyMove(const StateInfo& st) {
  // Seite zurück
  m_state.sideToMove = ~m_state.sideToMove;
  hashXorSide();
  if (m_state.sideToMove == core::Color::Black) --m_state.fullmoveNumber;

  // Castling-Rechte zurück
  hashSetCastling(m_state.castlingRights, st.prevCastlingRights);
  m_state.castlingRights = st.prevCastlingRights;

  // EP zurück in Hash integrieren
  m_state.enPassantSquare = st.prevEnPassantSquare;
  xorEPRelevant();

  // 50-Züge
  m_state.halfmoveClock = st.prevHalfmoveClock;

  // Figuren zurücksetzen
  const Move& m = st.move;
  core::Color us = m_state.sideToMove;  // Spieler, der den Zug gemacht hatte

  // Rochade rückgängig
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

  // ziehende Figur zurück auf "from" (Promos werden zu Bauern zurückverwandelt)
  if (auto moving = m_board.getPiece(m.to)) {
    m_board.removePiece(m.to);
    bb::Piece placed = *moving;
    if (m.promotion != core::PieceType::None) placed.type = core::PieceType::Pawn;

    hashXorPiece(us, moving->type, m.to);
    hashXorPiece(us, placed.type, m.from);
    m_board.setPiece(m.from, placed);
  } else {
    // Defensive: Inkonsistenz – brich sauber ab (keine Exceptions/ctz auf 0)
    // Hash und PawnKey werden vom Aufrufer (undoMove/doMove-Illegalität) zurückgesetzt.
    return;
  }

  // Capture rückgängig machen
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

// ---------------------- Castling Rights Helper ----------------------

void Position::updateCastlingRightsOnMove(core::Square from, core::Square to) {
  auto clear = [&](std::uint8_t rights) { m_state.castlingRights &= ~rights; };

  // König zieht oder wird geschlagen
  if (from == bb::E1 || to == bb::E1) clear(bb::Castling::WK | bb::Castling::WQ);
  if (from == bb::E8 || to == bb::E8) clear(bb::Castling::BK | bb::Castling::BQ);

  // Türme ziehen oder werden geschlagen
  if (from == bb::H1 || to == bb::H1) clear(bb::Castling::WK);
  if (from == bb::A1 || to == bb::A1) clear(bb::Castling::WQ);
  if (from == bb::H8 || to == bb::H8) clear(bb::Castling::BK);
  if (from == bb::A8 || to == bb::A8) clear(bb::Castling::BQ);
}

}  // namespace lilia::model
