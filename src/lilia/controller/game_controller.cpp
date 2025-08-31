#include "lilia/controller/game_controller.hpp"

#include <SFML/System/Sleep.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <algorithm>
#include <iostream>
#include <string>

#include "lilia/controller/bot_player.hpp"
#include "lilia/controller/game_manager.hpp"
#include "lilia/model/chess_game.hpp"
#include "lilia/model/move.hpp"
#include "lilia/uci/uci_helper.hpp"
#include "lilia/view/render_constants.hpp"

namespace lilia::controller {

namespace {
// Kleiner Helfer: safe-compare für Squares
inline bool isValid(core::Square sq) {
  return sq != core::NO_SQUARE;
}
}  // namespace

GameController::GameController(view::GameView &gView, model::ChessGame &game)
    : m_game_view(gView), m_chess_game(game) {
  m_input_manager.setOnClick([this](core::MousePos pos) { this->onClick(pos); });
  m_input_manager.setOnDrag(
      [this](core::MousePos start, core::MousePos current) { this->onDrag(start, current); });
  m_input_manager.setOnDrop(
      [this](core::MousePos start, core::MousePos end) { this->onDrop(start, end); });

  m_sound_manager.loadSounds();

  // GameManager
  m_game_manager = std::make_unique<GameManager>(game);
  BotPlayer::setEvalCallback([this](int eval) { m_eval_cp.store(eval); });

  // Move-Callback – alles GUI/Animationen laufen über diese eine Stelle.
  m_game_manager->setOnMoveExecuted([this](const model::Move &mv, bool isPlayerMove, bool onClick) {
    this->movePieceAndClear(mv, isPlayerMove, onClick);
    this->m_chess_game.checkGameResult();
    this->m_game_view.addMove(move_to_uci(mv));
    this->m_fen_history.push_back(this->m_chess_game.getFen());
    this->m_move_history.emplace_back(mv.from, mv.to);
    this->m_fen_index = this->m_fen_history.size() - 1;
    this->m_game_view.setBoardFen(this->m_fen_history.back());
    this->highlightLastMove();
    this->m_game_view.selectMove(this->m_fen_index ? this->m_fen_index - 1
                                                   : static_cast<std::size_t>(-1));
  });

  m_game_manager->setOnPromotionRequested([this](core::Square sq) {
    this->m_game_view.playPromotionSelectAnim(sq, m_chess_game.getGameState().sideToMove);
  });

  m_game_manager->setOnGameEnd([this](core::GameResult res) {
    this->m_game_view.showGameOver(res, m_chess_game.getGameState().sideToMove);
    this->m_sound_manager.playGameEnds();
  });
}

GameController::~GameController() = default;

void GameController::startGame(const std::string &fen, bool whiteIsBot, bool blackIsBot,
                               int think_time_ms, int depth) {
  m_sound_manager.playGameBegins();
  m_game_view.init(fen);
  m_game_view.setBotMode(whiteIsBot || blackIsBot);
  m_game_manager->startGame(fen, whiteIsBot, blackIsBot, think_time_ms, depth);

  m_fen_history.clear();
  m_fen_history.push_back(fen);
  m_fen_index = 0;
  m_move_history.clear();
  m_game_view.selectMove(static_cast<std::size_t>(-1));

  // UI-State
  m_mouse_down = false;
  m_dragging = false;
  m_drag_from = core::NO_SQUARE;
  m_preview_active = false;
  m_prev_selected_before_preview = core::NO_SQUARE;
  m_selected_sq = core::NO_SQUARE;
  m_hover_sq = core::NO_SQUARE;
  m_last_move_squares = {core::NO_SQUARE, core::NO_SQUARE};

  m_game_view.setDefaultCursor();
}

void GameController::handleEvent(const sf::Event &event) {
  if (m_chess_game.getResult() != core::GameResult::ONGOING) return;

  if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
    std::size_t idx =
        m_game_view.getMoveIndexAt(core::MousePos(event.mouseButton.x, event.mouseButton.y));
    if (idx != static_cast<std::size_t>(-1)) {
      m_fen_index = idx + 1;
      m_game_view.setBoardFen(m_fen_history[m_fen_index]);
      m_game_view.selectMove(idx);
      m_last_move_squares = m_move_history[idx];
      m_game_view.clearAllHighlights();
      highlightLastMove();
      bool whiteMoved = (m_fen_index % 2 == 1);
      if (whiteMoved)
        m_sound_manager.playPlayerMove();
      else
        m_sound_manager.playEnemyMove();
      return;
    }
  }

  if (event.type == sf::Event::MouseWheelScrolled) {
    m_game_view.scrollMoveList(event.mouseWheelScroll.delta);
    if (m_fen_index != m_fen_history.size() - 1) return;
  }

  if (event.type == sf::Event::KeyPressed) {
    if (event.key.code == sf::Keyboard::Left) {
      if (m_fen_index > 0) {
        --m_fen_index;
        m_game_view.setBoardFen(m_fen_history[m_fen_index]);
        if (m_fen_index == 0) {
          m_game_view.selectMove(static_cast<std::size_t>(-1));
          m_last_move_squares = {core::NO_SQUARE, core::NO_SQUARE};
        } else {
          m_game_view.selectMove(m_fen_index - 1);
          m_last_move_squares = m_move_history[m_fen_index - 1];
        }
        m_game_view.clearAllHighlights();
        highlightLastMove();
        bool whiteMoved = (m_fen_index % 2 == 1);
        if (whiteMoved)
          m_sound_manager.playPlayerMove();
        else
          m_sound_manager.playEnemyMove();
      }
      return;
    } else if (event.key.code == sf::Keyboard::Right) {
      if (m_fen_index + 1 < m_fen_history.size()) {
        ++m_fen_index;
        m_game_view.setBoardFen(m_fen_history[m_fen_index]);
        m_game_view.selectMove(m_fen_index - 1);
        m_last_move_squares = m_move_history[m_fen_index - 1];
        m_game_view.clearAllHighlights();
        highlightLastMove();
        bool whiteMoved = (m_fen_index % 2 == 1);
        if (whiteMoved)
          m_sound_manager.playPlayerMove();
        else
          m_sound_manager.playEnemyMove();
      }
      return;
    }
  }
  if (m_fen_index != m_fen_history.size() - 1) return;

  switch (event.type) {
    case sf::Event::MouseMoved:
      onMouseMove(core::MousePos(event.mouseMove.x, event.mouseMove.y));
      break;
    case sf::Event::MouseButtonPressed:
      if (event.mouseButton.button == sf::Mouse::Left)
        onMousePressed(core::MousePos(event.mouseButton.x, event.mouseButton.y));
      break;
    case sf::Event::MouseButtonReleased:
      if (event.mouseButton.button == sf::Mouse::Left)
        onMouseReleased(core::MousePos(event.mouseButton.x, event.mouseButton.y));
      break;
    default:
      break;
  }
  m_input_manager.processEvent(event);
}

void GameController::onMouseMove(core::MousePos pos) {
  if (m_dragging) {
    m_game_view.setHandClosedCursor();
    return;
  }

  if (m_mouse_down) {
    m_game_view.setHandClosedCursor();
    return;
  }

  const core::Square sq = m_game_view.mousePosToSquare(pos);
  if (m_game_view.hasPieceOnSquare(sq) && !m_game_view.isInPromotionSelection())
    m_game_view.setHandOpenCursor();
  else
    m_game_view.setDefaultCursor();
}

void GameController::onMousePressed(core::MousePos pos) {
  m_mouse_down = true;

  if (m_game_view.isInPromotionSelection()) {
    m_game_view.setHandClosedCursor();
    return;
  }

  const core::Square sq = m_game_view.mousePosToSquare(pos);

  if (m_game_view.hasPieceOnSquare(sq)) {
    // Erst versuchen, ob ein Klick-Zug von der aktuellen Auswahl möglich ist.
    if (!tryMove(m_selected_sq, sq)) m_game_view.setHandClosedCursor();
  } else {
    m_game_view.setDefaultCursor();
  }

  if (!m_game_view.hasPieceOnSquare(sq)) return;

  // Preview-Logik: Wechsel der Auswahl neu highlighten
  if (m_selected_sq != core::NO_SQUARE && m_selected_sq != sq) {
    m_preview_active = true;
    m_prev_selected_before_preview = m_selected_sq;

    if (!tryMove(m_selected_sq, sq)) {
      m_game_view.clearAllHighlights();
      highlightLastMove();
      selectSquare(sq);
      hoverSquare(sq);
      showAttacks(getAttackSquares(sq));
    }
  } else {
    m_preview_active = false;
    m_prev_selected_before_preview = core::NO_SQUARE;

    m_game_view.clearAllHighlights();
    highlightLastMove();
    selectSquare(sq);
    hoverSquare(sq);
    showAttacks(getAttackSquares(sq));
  }

  // Drag starten, wenn nicht direkt Klick-Zug möglich war
  if (!tryMove(m_selected_sq, sq)) {
    m_dragging = true;
    m_drag_from = sq;
    m_game_view.setPieceToMouseScreenPos(sq, pos);
    m_game_view.playPiecePlaceHolderAnimation(sq);
  }
}

void GameController::onMouseReleased(core::MousePos pos) {
  m_mouse_down = false;

  if (m_dragging) {
    m_dragging = false;
    m_drag_from = core::NO_SQUARE;
  }

  m_preview_active = false;
  m_prev_selected_before_preview = core::NO_SQUARE;

  onMouseMove(pos);
}

void GameController::render() {
  m_game_view.render();
}

void GameController::update(float dt) {
  if (m_chess_game.getResult() != core::GameResult::ONGOING) return;

  m_game_view.update(dt);
  m_game_view.updateEval(m_eval_cp.load());
  if (m_game_manager) m_game_manager->update(dt);
}

void GameController::highlightLastMove() {
  if (isValid(m_last_move_squares.first)) m_game_view.highlightSquare(m_last_move_squares.first);
  if (isValid(m_last_move_squares.second)) m_game_view.highlightSquare(m_last_move_squares.second);
}

void GameController::selectSquare(core::Square sq) {
  m_game_view.highlightSquare(sq);
  m_selected_sq = sq;
}

void GameController::deselectSquare() {
  m_game_view.clearAllHighlights();
  highlightLastMove();
  m_selected_sq = core::NO_SQUARE;
}

void GameController::hoverSquare(core::Square sq) {
  m_hover_sq = sq;
  m_game_view.highlightHoverSquare(m_hover_sq);
}

void GameController::dehoverSquare() {
  if (isValid(m_hover_sq)) m_game_view.clearHighlightHoverSquare(m_hover_sq);
  m_hover_sq = core::NO_SQUARE;
}

// --------- ZENTRALE Move-Callback-Behandlung (auch Engine-Züge) ----------
void GameController::movePieceAndClear(const model::Move &move, bool isPlayerMove, bool onClick) {
  const core::Square from = move.from;
  const core::Square to = move.to;

  // 1) Drag-Konflikt defensiv auflösen
  if (m_dragging && m_drag_from == from) {
    m_dragging = false;
    m_mouse_down = false;
    dehoverSquare();

    // Visuell zurück auf "from" klipsen und laufende Platzhalter/Drags beenden
    m_game_view.setPieceToSquareScreenPos(from, from);
    m_game_view.endAnimation(
        from);  // beendet Base-Layer; (Highlight-Layer wird vom AnimMgr ersetzt)
  }

  // 2) Auswahl-Konflikte entschärfen
  if (m_selected_sq == from || m_selected_sq == to) {
    deselectSquare();
  }
  m_preview_active = false;
  m_prev_selected_before_preview = core::NO_SQUARE;

  // 3) En-passant Opferfeld bestimmen (für Animation)
  core::Square epVictimSq = core::NO_SQUARE;
  const core::Color moverColorBefore = ~m_chess_game.getGameState().sideToMove;
  if (move.isEnPassant) {
    epVictimSq = (moverColorBefore == core::Color::White) ? static_cast<core::Square>(to - 8)
                                                          : static_cast<core::Square>(to + 8);
  }

  // 4) Los geht’s: Animationsauswahl je nach Eingabeart (Klick vs. Drag)
  if (onClick)
    m_game_view.animationMovePiece(from, to, epVictimSq, move.promotion);
  else
    m_game_view.animationDropPiece(from, to, epVictimSq, move.promotion);

  // 5) Rochade animieren
  if (move.castle != model::CastleSide::None) {
    const core::Square rookFrom =
        m_chess_game.getRookSquareFromCastleside(move.castle, moverColorBefore);
    const core::Square rookTo = (move.castle == model::CastleSide::KingSide)
                                    ? static_cast<core::Square>(to - 1)
                                    : static_cast<core::Square>(to + 1);
    m_game_view.animationMovePiece(rookFrom, rookTo);
  }

  // 6) Visuals / Sounds
  m_last_move_squares = {from, to};
  deselectSquare();
  highlightLastMove();

  const core::Color sideToMoveNow = m_chess_game.getGameState().sideToMove;

  if (m_chess_game.isKingInCheck(sideToMoveNow)) {
    m_sound_manager.playCheck();
  } else {
    if (move.promotion != core::PieceType::None) {
      m_sound_manager.playPromotion();
    } else if (move.isCapture) {
      m_sound_manager.playCapture();
    } else if (move.castle == model::CastleSide::None) {
      if (isPlayerMove)
        m_sound_manager.playPlayerMove();
      else
        m_sound_manager.playEnemyMove();
    } else {
      m_sound_manager.playCastle();
    }
  }
}

// ------------------------------------------------------------------------

void GameController::snapAndReturn(core::Square sq, core::MousePos cur) {
  selectSquare(sq);
  m_game_view.animationSnapAndReturn(sq, cur);
}

[[nodiscard]] bool GameController::tryMove(core::Square a, core::Square b) {
  for (auto att : getAttackSquares(a)) {
    if (att == b) return true;
  }
  return false;
}

[[nodiscard]] bool GameController::isPromotion(core::Square a, core::Square b) {
  for (const auto &m : m_chess_game.generateLegalMoves()) {
    if (m.from == a && m.to == b && m.promotion != core::PieceType::None) return true;
  }
  return false;
}

[[nodiscard]] bool GameController::isSameColor(core::Square a, core::Square b) {
  return m_game_view.isSameColorPiece(a, b);
}

[[nodiscard]] std::vector<core::Square> GameController::getAttackSquares(
    core::Square pieceSQ) const {
  std::vector<core::Square> att;
  for (const auto &m : m_chess_game.generateLegalMoves()) {
    if (m.from == pieceSQ) att.push_back(m.to);
  }
  return att;
}

void GameController::showAttacks(std::vector<core::Square> att) {
  if (!m_game_manager || !m_game_manager->isHumanTurn()) return;
  for (auto sq : att) {
    if (m_game_view.hasPieceOnSquare(sq))
      m_game_view.highlightCaptureSquare(sq);
    else
      m_game_view.highlightAttackSquare(sq);
  }
}

void GameController::onClick(core::MousePos mousePos) {
  if (m_game_view.isOnFlipIcon(mousePos)) {
    m_game_view.toggleBoardOrientation();
    return;
  }
  const core::Square sq = m_game_view.mousePosToSquare(mousePos);

  // Promotion-Auswahl?
  if (m_game_view.isInPromotionSelection()) {
    const core::PieceType promoType = m_game_view.getSelectedPromotion(mousePos);
    m_game_view.removePromotionSelection();
    if (m_game_manager) m_game_manager->completePendingPromotion(promoType);
    deselectSquare();
    return;
  }

  // Bereits etwas selektiert? -> erst Zug versuchen (hat Vorrang)
  if (m_selected_sq != core::NO_SQUARE) {
    const auto st = m_chess_game.getGameState();
    const bool ownTurnAndPiece = (st.sideToMove == m_chess_game.getPiece(m_selected_sq).color) &&
                                 (!m_game_manager || m_game_manager->isHuman(st.sideToMove));

    if (ownTurnAndPiece && tryMove(m_selected_sq, sq)) {
      if (m_game_manager) {
        (void)m_game_manager->requestUserMove(m_selected_sq, sq,
                                              /*onClick*/ true);
      }
      return;  // NICHT umselektieren
    }

    // Kein legaler Klick-Zug -> ggf. Auswahl ändern/entfernen
    if (m_game_view.hasPieceOnSquare(sq)) {
      m_game_view.clearAllHighlights();
      highlightLastMove();
      selectSquare(sq);
      showAttacks(getAttackSquares(sq));
    } else {
      deselectSquare();
    }
    return;
  }

  // Nichts selektiert: ggf. neue Auswahl
  if (m_game_view.hasPieceOnSquare(sq)) {
    m_game_view.clearAllHighlights();
    highlightLastMove();
    selectSquare(sq);
    showAttacks(getAttackSquares(sq));
  }
}

void GameController::onDrag(core::MousePos start, core::MousePos current) {
  const core::Square sqStart = m_game_view.mousePosToSquare(start);
  const core::Square sqMous = m_game_view.mousePosToSquare(current);

  if (m_game_view.isInPromotionSelection()) return;
  if (!m_game_view.hasPieceOnSquare(sqStart)) return;
  if (!m_dragging) return;

  // Sicherstellen, dass die Startfigur selektiert ist
  if (m_selected_sq != sqStart) {
    m_game_view.clearAllHighlights();
    highlightLastMove();
    selectSquare(sqStart);
    showAttacks(getAttackSquares(sqStart));
  }

  if (m_hover_sq != sqMous) dehoverSquare();
  hoverSquare(sqMous);

  m_game_view.setPieceToMouseScreenPos(sqStart, current);
  m_game_view.playPiecePlaceHolderAnimation(sqStart);
}

void GameController::onDrop(core::MousePos start, core::MousePos end) {
  const core::Square from = m_game_view.mousePosToSquare(start);
  const core::Square to = m_game_view.mousePosToSquare(end);

  dehoverSquare();

  if (m_game_view.isInPromotionSelection()) return;

  if (!m_game_view.hasPieceOnSquare(from)) {
    deselectSquare();
    m_preview_active = false;
    m_prev_selected_before_preview = core::NO_SQUARE;
    return;
  }

  // Platzhalter/Drag-Anim beenden, bevor wir irgendwas Neues tun
  m_game_view.endAnimation(from);

  bool accepted = false;
  if (from != to && tryMove(from, to) && m_game_manager) {
    accepted = m_game_manager->requestUserMove(from, to, /*onClick*/ false);
  }

  if (!accepted) {
    // Fehlversuch -> zurückschnappen
    if (m_chess_game.isKingInCheck(m_chess_game.getGameState().sideToMove) && m_game_manager &&
        m_game_manager->isHuman(m_chess_game.getGameState().sideToMove) && from != to &&
        m_game_view.hasPieceOnSquare(from) &&
        m_chess_game.getPiece(from).color == m_chess_game.getGameState().sideToMove) {
      m_game_view.warningKingSquareAnim(
          m_chess_game.getKingSquare(m_chess_game.getGameState().sideToMove));
      m_sound_manager.playWarning();
    }

    m_game_view.setPieceToSquareScreenPos(from, from);
    m_game_view.animationSnapAndReturn(from, end);

    if (m_preview_active && isValid(m_prev_selected_before_preview) &&
        m_prev_selected_before_preview != from) {
      m_game_view.clearAllHighlights();
      highlightLastMove();
      selectSquare(m_prev_selected_before_preview);
      showAttacks(getAttackSquares(m_prev_selected_before_preview));
    } else {
      m_game_view.clearAllHighlights();
      highlightLastMove();
      selectSquare(from);
      showAttacks(getAttackSquares(from));
    }
  }

  // Preview immer aufräumen
  m_preview_active = false;
  m_prev_selected_before_preview = core::NO_SQUARE;
}

}  // namespace lilia::controller
