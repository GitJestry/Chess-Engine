#include "lilia/model/position.hpp"

#include "lilia/engine/config.hpp"
#include "lilia/model/core/magic.hpp"
#include "lilia/model/move_generator.hpp"

namespace lilia::model {

bool Position::checkInsufficientMaterial() {
  // 1. Prüfe, ob es Bauern, Damen oder Türme gibt
  bb::Bitboard occ = 0;
  for (auto pt : {core::PieceType::Pawn, core::PieceType::Queen, core::PieceType::Rook}) {
    occ |= m_board.pieces(core::Color::White, pt);
    occ |= m_board.pieces(core::Color::Black, pt);
  }
  if (bb::popcount(occ) > 0) return false;  // genügend Material vorhanden

  // 2. Zähle Läufer und Springer
  bb::Bitboard whiteB = m_board.pieces(core::Color::White, core::PieceType::Bishop);
  bb::Bitboard blackB = m_board.pieces(core::Color::Black, core::PieceType::Bishop);
  bb::Bitboard whiteN = m_board.pieces(core::Color::White, core::PieceType::Knight);
  bb::Bitboard blackN = m_board.pieces(core::Color::Black, core::PieceType::Knight);

  int totalB = bb::popcount(whiteB) + bb::popcount(blackB);
  int totalN = bb::popcount(whiteN) + bb::popcount(blackN);

  // 3. Triviale Remisfälle
  if (totalB == 0 && totalN == 0) return true;  // nur Könige
  if (totalB == 0 && totalN == 1) return true;  // König + Springer vs König
  if (totalB == 1 && totalN == 0) return true;  // König + Läufer vs König

  // 4. König + Läufer vs König + Läufer auf gleicher Farbe
  if (totalB == 2 && totalN == 0) {
    auto whiteSq = static_cast<core::Square>(bb::ctz64(whiteB));
    auto blackSq = static_cast<core::Square>(bb::ctz64(blackB));
    if ((whiteSq % 2 == blackSq % 2)) return true;  // beide Läufer auf gleicher Farbe
  }

  // 5. König + Springer vs König + Springer
  if (totalB == 0 && totalN == 2) return true;

  return false;
}

bool Position::inCheck() const {
  bb::Bitboard kbb = m_board.pieces(m_state.sideToMove, core::PieceType::King);
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
      if (++count >= 2)  // 2 Treffer + aktuelle Stellung = 3-fach
        return true;
    }
  }
  return false;
}

// Simple -> improved SEE (static exchange evaluation).
// Returns true if the capture/promotion m is likely profitable or at least non-losing.
bool Position::see(const model::Move& m) const {
  // If neither capture nor promotion, no interest for SEE
  if (!m.isCapture && m.promotion == core::PieceType::None) return false;

  // get occupancy and piece bitboards
  bb::Bitboard occ = m_board.allPieces();

  // Determine initial captured piece value and capture-square
  int captured_value = 0;
  core::Square capture_sq = m.to;
  if (m.isEnPassant) {
    // en-passant captured pawn sits behind the to-square
    core::Square capSq = (m_board.getPiece(m.from).value().color == core::Color::White)
                             ? static_cast<core::Square>(m.to - 8)
                             : static_cast<core::Square>(m.to + 8);
    capture_sq = capSq;
    // captured pawn
    captured_value = engine::base_value[static_cast<int>(core::PieceType::Pawn)];
  } else if (m.isCapture) {
    auto cap = m_board.getPiece(m.to);
    if (cap)
      captured_value = engine::base_value[static_cast<int>(cap->type)];
    else
      captured_value = 0;
  } else {
    // non-capture promotion -> no captured piece
    captured_value = 0;
  }

  // Determine attacker type / value (the moving piece)
  core::PieceType attackerType = core::PieceType::Pawn;
  {
    auto p = m_board.getPiece(m.from);
    if (!p) return false;  // sanity
    attackerType = (m.promotion != core::PieceType::None) ? m.promotion : p->type;
  }
  const auto piece_value = [&](core::PieceType pt) -> int {
    return engine::base_value[static_cast<int>(pt)];
  };

  // Standard SEE algorithm:
  // gains[0] = value(captured)
  // then alternate least valuable attackers until no more attackers
  // finally propagate backwards: gains[i] = max(-gains[i+1], gains[i])
  // return gains[0] >= 0

  // side to move is the side that executes the first capture (the mover)
  core::Color side = m_board.getPiece(m.from).value().color;
  core::Color opponent = ~side;

  // make arrays of piece bitboards for quick checks
  std::array<bb::Bitboard, 6> wbbs{}, bbbs{};
  for (int pt = 0; pt < 6; ++pt) {
    wbbs[pt] = m_board.pieces(core::Color::White, static_cast<core::PieceType>(pt));
    bbbs[pt] = m_board.pieces(core::Color::Black, static_cast<core::PieceType>(pt));
  }

  // remove the moving piece from its from square because after the initial capture it will be on
  // the target
  occ &= ~bb::sq_bb(static_cast<core::Square>(m.from));

  // But assume attacker moves to capture square -> treat it as present on target for subsequent
  // immunity to being captured by other attackers? For SEE we model captures by removing attackers
  // from occ stepwise. initial captured value:
  std::vector<int> gains;
  gains.reserve(32);
  gains.push_back(captured_value);

  // we'll maintain mutable occ and recompute attackers each step
  bb::Bitboard occ_local = occ;

  // also mark that the moving piece is now on the capture square (for subsequent slide blockers we
  // do not need to place it, because in the first removal step we will remove the attacker we just
  // placed; many SEE implementations instead temporarily set occ with attacker on target. To be
  // safe for sliding attackers detection, add the attacker at target:
  occ_local |= bb::sq_bb(capture_sq);

  // helper to find least valuable attacker of `who` at square sq given occ_local
  auto find_least_valuable_attacker = [&](core::Color who, core::Square sq) -> int {
    // order by increasing value: Pawn, Knight, Bishop, Rook, Queen, King
    // return square index of least valuable attacker, or -1 if none
    auto check_and_get = [&](bb::Bitboard src_bb, auto attack_fn) -> int {
      bb::Bitboard bb = src_bb;
      while (bb) {
        int s = bb::ctz64(bb);
        bb &= bb - 1;
        // if this piece attacks sq given occupancy
        if (attack_fn(static_cast<core::Square>(s), sq, occ_local)) return s;
      }
      return -1;
    };

    // Pawn
    bb::Bitboard pawns = (who == core::Color::White)
                             ? wbbs[static_cast<int>(core::PieceType::Pawn)]
                             : bbbs[static_cast<int>(core::PieceType::Pawn)];
    // pawn attack test: does pawn on s attack sq?
    auto pawn_attacks_from = [&](core::Square s, core::Square target, bb::Bitboard) -> bool {
      if (who == core::Color::White) {
        // white pawn attacks s+7 or s+9 (if on board)
        bb::Bitboard attacked = bb::white_pawn_attacks(bb::sq_bb(s));
        return (attacked & bb::sq_bb(target)) != 0;
      } else {
        bb::Bitboard attacked = bb::black_pawn_attacks(bb::sq_bb(s));
        return (attacked & bb::sq_bb(target)) != 0;
      }
    };
    int sqPawn = check_and_get(pawns, pawn_attacks_from);
    if (sqPawn >= 0) return sqPawn;

    // Knight
    bb::Bitboard knights = (who == core::Color::White)
                               ? wbbs[static_cast<int>(core::PieceType::Knight)]
                               : bbbs[static_cast<int>(core::PieceType::Knight)];
    auto knight_att = [&](core::Square s, core::Square target, bb::Bitboard) -> bool {
      return (bb::knight_attacks_from(s) & bb::sq_bb(target)) != 0;
    };
    int sqKnight = check_and_get(knights, knight_att);
    if (sqKnight >= 0) return sqKnight;

    // Bishop (sliders)
    bb::Bitboard bishops = (who == core::Color::White)
                               ? wbbs[static_cast<int>(core::PieceType::Bishop)]
                               : bbbs[static_cast<int>(core::PieceType::Bishop)];
    auto bishop_att = [&](core::Square s, core::Square target, bb::Bitboard occ_) -> bool {
      return (magic::sliding_attacks(magic::Slider::Bishop, s, occ_) & bb::sq_bb(target)) != 0;
    };
    int sqB = check_and_get(bishops, bishop_att);
    if (sqB >= 0) return sqB;

    // Rook
    bb::Bitboard rooks = (who == core::Color::White)
                             ? wbbs[static_cast<int>(core::PieceType::Rook)]
                             : bbbs[static_cast<int>(core::PieceType::Rook)];
    auto rook_att = [&](core::Square s, core::Square target, bb::Bitboard occ_) -> bool {
      return (magic::sliding_attacks(magic::Slider::Rook, s, occ_) & bb::sq_bb(target)) != 0;
    };
    int sqR = check_and_get(rooks, rook_att);
    if (sqR >= 0) return sqR;

    // Queen
    bb::Bitboard queens = (who == core::Color::White)
                              ? wbbs[static_cast<int>(core::PieceType::Queen)]
                              : bbbs[static_cast<int>(core::PieceType::Queen)];
    auto queen_att = [&](core::Square s, core::Square target, bb::Bitboard occ_) -> bool {
      return (magic::sliding_attacks(magic::Slider::Rook, s, occ_) & bb::sq_bb(target)) != 0 ||
             (magic::sliding_attacks(magic::Slider::Bishop, s, occ_) & bb::sq_bb(target)) != 0;
    };
    int sqQ = check_and_get(queens, queen_att);
    if (sqQ >= 0) return sqQ;

    // King (rare, but include)
    bb::Bitboard kingb = (who == core::Color::White)
                             ? wbbs[static_cast<int>(core::PieceType::King)]
                             : bbbs[static_cast<int>(core::PieceType::King)];
    auto king_att = [&](core::Square s, core::Square target, bb::Bitboard) -> bool {
      return (bb::king_attacks_from(s) & bb::sq_bb(target)) != 0;
    };
    int sqK = check_and_get(kingb, king_att);
    if (sqK >= 0) return sqK;

    return -1;
  };

  // First attacker is the moving piece itself on the target: we consider that as already used.
  // To model this, we will explicitly remove the moving piece from the side's bitboard
  // (we already removed from its from-square; when it captures it's effectively on the target)
  // We'll treat that as the first removal step below.

  // Keep track of piece bitboards mutable copies for recomputing attackers each iteration:
  auto wbbs_mut = wbbs;
  auto bbbs_mut = bbbs;

  // If the move is a promotion and not a capture, there's no opponent capture; SEE should probably
  // return true for Q promotion but we'll still run through generic logic - promotion increases
  // attacker value.

  // Initially we already considered that attacker moved to capture square; next attacker is
  // opponent's least val attacker that attacks target.
  core::Color cur = opponent;  // next to move to recapture

  // Remove the piece that just moved from its original bitboard; it's now effectively the attacker
  // at target We already removed it from occ_local when we cleared m.from; but we must also remove
  // from the appropriate color bitboard:
  auto& from_board = (side == core::Color::White) ? wbbs_mut : bbbs_mut;
  // find and clear the bit corresponding to 'm.from'
  from_board[static_cast<int>(m_board.getPiece(m.from).value().type)] &= ~bb::sq_bb(m.from);
  // then add the attacker to target in the same color bitboard under its final (promoted) type:
  core::PieceType final_att_type =
      (m.promotion != core::PieceType::None) ? m.promotion : m_board.getPiece(m.from).value().type;
  from_board[static_cast<int>(final_att_type)] |= bb::sq_bb(capture_sq);

  // Now the iterative capture-exchange sequence:
  for (;;) {
    int attacker_sq = find_least_valuable_attacker(cur, capture_sq);
    if (attacker_sq < 0) break;

    // determine attacker piece type at attacker_sq
    core::PieceType atype = core::PieceType::Pawn;
    // find which piece bitboard contains attacker_sq:
    for (int pt = 0; pt < 6; ++pt) {
      if (cur == core::Color::White) {
        if (wbbs_mut[pt] & bb::sq_bb(static_cast<core::Square>(attacker_sq))) {
          atype = static_cast<core::PieceType>(pt);
          wbbs_mut[pt] &= ~bb::sq_bb(static_cast<core::Square>(attacker_sq));
          break;
        }
      } else {
        if (bbbs_mut[pt] & bb::sq_bb(static_cast<core::Square>(attacker_sq))) {
          atype = static_cast<core::PieceType>(pt);
          bbbs_mut[pt] &= ~bb::sq_bb(static_cast<core::Square>(attacker_sq));
          break;
        }
      }
    }

    // remove attacker from occupancy
    occ_local &= ~bb::sq_bb(static_cast<core::Square>(attacker_sq));

    // compute value of this attacker
    int v = piece_value(atype);
    gains.push_back(v - gains.back());

    // next side to move
    cur = ~cur;
  }

  // minimax propagation of gains
  int n = static_cast<int>(gains.size());
  for (int i = n - 2; i >= 0; --i) {
    gains[i] = std::max(-gains[i + 1], gains[i]);
  }

  return gains.empty() ? false : (gains[0] >= 0);
}

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
    if (magic::sliding_attacks(magic::Slider::Bishop, s, occ) & bb::sq_bb(sq)) return true;
  }

  // rooks/queens
  bb::Bitboard rooks =
      m_board.pieces(by, core::PieceType::Rook) | m_board.pieces(by, core::PieceType::Queen);
  for (bb::Bitboard r = rooks; r;) {
    core::Square s = bb::pop_lsb(r);

    if (magic::sliding_attacks(magic::Slider::Rook, s, occ) & bb::sq_bb(sq)) return true;
  }

  // king
  bb::Bitboard king = m_board.pieces(by, core::PieceType::King);
  if (king && (bb::king_attacks_from(static_cast<core::Square>(bb::ctz64(king))) & bb::sq_bb(sq)))
    return true;

  return false;
}

bool Position::doMove(const Move& m) {
  // Vorprüfungen für spezielle Züge (Rochade)
  core::Color us = m_state.sideToMove;
  core::Color them = ~us;

  // --- CASTLING: prüfe Vorbedingungen bevor applyMove() ausgeführt wird ---
  if (m.castle != CastleSide::None) {
    // König-Position aktuell bestimmen
    bb::Bitboard kbb = m_board.pieces(us, core::PieceType::King);
    if (!kbb) return false;  // kein König - inkonsistent
    core::Square ksq = static_cast<core::Square>(bb::ctz64(kbb));

    // Der Move sollte vom König ausgehen
    if (m.from != ksq) return false;

    // 1) König darf vor dem Zug nicht im Schach stehen
    if (isSquareAttacked(ksq, them)) return false;

    // 2) Castling-Rechte prüfen (falls Move-Generator das nicht garantiert)
    std::uint8_t cr = m_state.castlingRights;
    if (us == core::Color::White) {
      if (m.castle == CastleSide::KingSide && !(cr & bb::Castling::WK)) return false;
      if (m.castle == CastleSide::QueenSide && !(cr & bb::Castling::WQ)) return false;
    } else {
      if (m.castle == CastleSide::KingSide && !(cr & bb::Castling::BK)) return false;
      if (m.castle == CastleSide::QueenSide && !(cr & bb::Castling::BQ)) return false;
    }

    // 3) Rook-Präsenz am Ecke kontrollieren (H1/A1 bzw H8/A8)
    core::Square rookSq = static_cast<core::Square>(
        (us == core::Color::White) ? (m.castle == CastleSide::KingSide ? bb::H1 : bb::A1)
                                   : (m.castle == CastleSide::KingSide ? bb::H8 : bb::A8));
    auto rookPiece = m_board.getPiece(rookSq);
    if (!rookPiece) return false;
    if (rookPiece->type != core::PieceType::Rook || rookPiece->color != us) return false;

    // 4) Zwischenfelder müssen leer sein (E1..H1 -> F1,G1 ; E1..A1 -> D1,C1,B1)
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

    // 5) König darf weder über noch auf ein angegriffenes Feld ziehen:
    //    prüfen: das Durchgangs- und Zielquadrat (z. B. F1 und G1)
    core::Square passSq1, passSq2;
    if (us == core::Color::White) {
      if (m.castle == CastleSide::KingSide) {
        passSq1 = static_cast<core::Square>(5);  // F1
        passSq2 = static_cast<core::Square>(6);  // G1
      } else {
        passSq1 = static_cast<core::Square>(3);  // D1
        passSq2 = static_cast<core::Square>(2);  // C1
      }
    } else {
      if (m.castle == CastleSide::KingSide) {
        passSq1 = static_cast<core::Square>(61);  // F8
        passSq2 = static_cast<core::Square>(62);  // G8
      } else {
        passSq1 = static_cast<core::Square>(59);  // D8
        passSq2 = static_cast<core::Square>(58);  // C8
      }
    }

    if (isSquareAttacked(passSq1, them) || isSquareAttacked(passSq2, them)) return false;
    // (Hinweis: Startfeld wurde oben geprüft)
  }

  // --- Ende der Vorprüfungen. Wenn OK, applyMove und finale Legality-Prüfung wie gehabt ---
  StateInfo st{};
  st.move = m;
  st.zobristKey = m_hash;  // aktuellen Hash merken
  st.prevCastlingRights = m_state.castlingRights;
  st.prevEnPassantSquare = m_state.enPassantSquare;
  st.prevHalfmoveClock = m_state.halfmoveClock;
  st.prevPawnKey = m_state.pawnKey;

  // apply
  applyMove(m, st);

  // legality: after apply, sideToMove has flipped
  core::Color us_after = ~m_state.sideToMove;
  bb::Bitboard kbb = m_board.pieces(us_after, core::PieceType::King);
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
  unapplyMove(st);
  m_hash = st.zobristKey;  // Hash zurücksetzen
  m_state.pawnKey = st.prevPawnKey;
  m_history.pop_back();
}

bool Position::doNullMove() {
  // Nullzug: keine Steine bewegen, nur State/Hash anpassen

  NullState st{};
  st.zobristKey = m_hash;
  st.prevCastlingRights = m_state.castlingRights;
  st.prevEnPassantSquare = m_state.enPassantSquare;
  st.prevHalfmoveClock = m_state.halfmoveClock;
  st.prevFullmoveNumber = m_state.fullmoveNumber;

  // EP aus Hash/State entfernen (wie bei echtem Zug)
  hashClearEP();
  m_state.enPassantSquare = static_cast<core::Square>(64);

  // 50-Züge-Regel: Nullzug ist weder Bauernzug noch Schlag → Halfmove++
  ++m_state.halfmoveClock;

  // Seite wechseln (Hash & State). Fullmove, wenn Schwarz → Weiß
  hashXorSide();
  m_state.sideToMove = ~m_state.sideToMove;
  if (m_state.sideToMove == core::Color::White) {
    ++m_state.fullmoveNumber;
  }

  m_nullHistory.push_back(st);
  return true;
}

void Position::undoNullMove() {
  if (m_nullHistory.empty()) return;

  NullState st = m_nullHistory.back();
  m_nullHistory.pop_back();

  // Seite zurückdrehen (erst State flippen, dann Hash wie im Vorwärtsgang gespiegelt)
  m_state.sideToMove = ~m_state.sideToMove;
  hashXorSide();

  // Fullmove wiederherstellen
  m_state.fullmoveNumber = st.prevFullmoveNumber;

  // EP wiederherstellen (Hash erst clear, dann ggf. set)
  hashClearEP();
  m_state.enPassantSquare = st.prevEnPassantSquare;
  hashSetEP(m_state.enPassantSquare);

  // Castling-Rechte (sollten unverändert sein, aber wir stellen explizit zurück)
  m_state.castlingRights = st.prevCastlingRights;

  // Halfmove-Clock zurück
  m_state.halfmoveClock = st.prevHalfmoveClock;

  // Zobrist-Key voll zurücksetzen (redundant zu obigen XORs, aber sicher & schnell)
  m_hash = st.zobristKey;
}

// Helpers
void Position::applyMove(const Move& m, StateInfo& st) {
  core::Color us = m_state.sideToMove;
  core::Color them = ~us;

  // Zobrist: side to move flips at end; we’ll xor at the end (like Stockfish)
  // First, clear EP (if any) in hash/state (EP depends only on file)
  hashClearEP();
  m_state.enPassantSquare = core::NO_SQUARE;

  // --- Capture Handling ---
  if (m.isEnPassant) {
    core::Square capSq = (us == core::Color::White) ? static_cast<core::Square>(m.to - 8)
                                                    : static_cast<core::Square>(m.to + 8);
    auto cap = m_board.getPiece(capSq);
    st.captured = cap.value_or(bb::Piece{core::PieceType::None, them});
    if (cap) {
      hashXorPiece(them, core::PieceType::Pawn, capSq);  // PawnKey korrekt togglen
      m_board.removePiece(capSq);
    }
  } else if (m.isCapture) {
    auto cap = m_board.getPiece(m.to);
    st.captured = cap.value_or(bb::Piece{core::PieceType::None, them});
    if (cap) {
      hashXorPiece(them, st.captured.type, m.to);  // PawnKey für Bauern
      m_board.removePiece(m.to);
    }
  } else {
    st.captured = bb::Piece{core::PieceType::None, them};
  }

  // --- Moving Piece ---
  auto moving = m_board.getPiece(m.from);
  bb::Piece placed = moving.value();      // muss existieren
  hashXorPiece(us, placed.type, m.from);  // entferne alten Platz

  m_board.removePiece(m.from);

  // Promotion
  if (m.promotion != core::PieceType::None) placed.type = m.promotion;

  hashXorPiece(us, placed.type, m.to);  // add neue Position
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

  // Halfmove-Clock
  if (placed.type == core::PieceType::Pawn || st.captured.type != core::PieceType::None)
    m_state.halfmoveClock = 0;
  else
    ++m_state.halfmoveClock;

  // En passant Square nach Double-Pawn-Move
  if (placed.type == core::PieceType::Pawn) {
    if (us == core::Color::White && bb::rank_of(m.from) == 1 && bb::rank_of(m.to) == 3)
      m_state.enPassantSquare = static_cast<core::Square>(m.from + 8);
    else if (us == core::Color::Black && bb::rank_of(m.from) == 6 && bb::rank_of(m.to) == 4)
      m_state.enPassantSquare = static_cast<core::Square>(m.from - 8);
  }

  hashSetEP(m_state.enPassantSquare);

  // Castling rights update
  std::uint8_t prevCR = m_state.castlingRights;
  updateCastlingRightsOnMove(m.from, m.to);
  if (prevCR != m_state.castlingRights) hashSetCastling(prevCR, m_state.castlingRights);

  // Flip side
  hashXorSide();
  m_state.sideToMove = them;
  if (them == core::Color::White) ++m_state.fullmoveNumber;
}

void Position::unapplyMove(const StateInfo& st) {
  // Side flips back
  m_state.sideToMove = ~m_state.sideToMove;
  hashXorSide();
  if (m_state.sideToMove == core::Color::Black) --m_state.fullmoveNumber;

  // Castling rights
  hashSetCastling(m_state.castlingRights, st.prevCastlingRights);
  m_state.castlingRights = st.prevCastlingRights;

  // EP
  hashClearEP();
  m_state.enPassantSquare = st.prevEnPassantSquare;
  hashSetEP(m_state.enPassantSquare);

  m_state.halfmoveClock = st.prevHalfmoveClock;

  const Move& m = st.move;
  core::Color us = m_state.sideToMove;

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

  // Move piece back
  auto moving = m_board.getPiece(m.to);
  if (moving) {
    m_board.removePiece(m.to);
    bb::Piece placed = *moving;
    if (m.promotion != core::PieceType::None)
      placed.type = core::PieceType::Pawn;  // undo promotion
    m_board.setPiece(m.from, placed);
  }

  // Restore captured
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
