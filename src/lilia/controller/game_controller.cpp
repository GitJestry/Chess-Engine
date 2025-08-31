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
#include "lilia/model/move_generator.hpp"
#include "lilia/model/position.hpp"
#include "lilia/uci/uci_helper.hpp"
#include "lilia/view/render_constants.hpp"

namespace lilia::controller {

namespace {
inline bool isValid(core::Square sq) { return sq != core::NO_SQUARE; }
} // namespace

GameController::GameController(view::GameView &gView, model::ChessGame &game)
    : m_game_view(gView), m_chess_game(game) {
  m_input_manager.setOnClick(
      [this](core::MousePos pos) { this->onClick(pos); });
  m_input_manager.setOnDrag(
      [this](core::MousePos start, core::MousePos current) {
        this->onDrag(start, current);
      });
  m_input_manager.setOnDrop([this](core::MousePos start, core::MousePos end) {
    this->onDrop(start, end);
  });

  m_sound_manager.loadSounds();

  m_game_manager = std::make_unique<GameManager>(game);
  BotPlayer::setEvalCallback([this](int eval) { m_eval_cp.store(eval); });

  m_game_manager->setOnMoveExecuted(
      [this](const model::Move &mv, bool isPlayerMove, bool onClick) {
        this->movePieceAndClear(mv, isPlayerMove, onClick);
        this->m_chess_game.checkGameResult();
        this->m_game_view.addMove(move_to_uci(mv));
        this->m_fen_history.push_back(this->m_chess_game.getFen());
        this->m_fen_index = this->m_fen_history.size() - 1;
        this->m_game_view.selectMove(this->m_fen_index
                                         ? this->m_fen_index - 1
                                         : static_cast<std::size_t>(-1));
      });

  m_game_manager->setOnPromotionRequested([this](core::Square sq) {
    this->m_game_view.playPromotionSelectAnim(
        sq, m_chess_game.getGameState().sideToMove);
  });

  m_game_manager->setOnGameEnd([this](core::GameResult res) {
    this->showGameOver(res, m_chess_game.getGameState().sideToMove);
    this->m_sound_manager.playEffect(view::sound::Effect::GameEnds);
  });
}

GameController::~GameController() = default;

void GameController::startGame(const std::string &fen, bool whiteIsBot,
                               bool blackIsBot, int think_time_ms, int depth) {
  m_sound_manager.playEffect(view::sound::Effect::GameBegins);
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
  m_selection_changed_on_press = false;

  // Premove + Auto-Move
  m_premove_from = core::NO_SQUARE;
  m_premove_to = core::NO_SQUARE;
  m_has_pending_auto_move = false;
  m_pending_from = core::NO_SQUARE;
  m_pending_to = core::NO_SQUARE;

  m_game_view.setDefaultCursor();
}

void GameController::handleEvent(const sf::Event &event) {
  if (m_chess_game.getResult() != core::GameResult::ONGOING)
    return;

  if (event.type == sf::Event::MouseButtonPressed &&
      event.mouseButton.button == sf::Mouse::Left) {
    std::size_t idx = m_game_view.getMoveIndexAt(
        core::MousePos(event.mouseButton.x, event.mouseButton.y));
    if (idx != static_cast<std::size_t>(-1)) {
      m_fen_index = idx + 1;
      m_game_view.setBoardFen(m_fen_history[m_fen_index]);
      m_game_view.selectMove(idx);
      const MoveView &info = m_move_history[idx];
      m_last_move_squares = {info.move.from, info.move.to};
      m_game_view.clearAllHighlights();
      highlightLastMove();
      m_sound_manager.playEffect(info.sound);
      return;
    }
  }

  if (event.type == sf::Event::MouseWheelScrolled) {
    m_game_view.scrollMoveList(event.mouseWheelScroll.delta);
    if (m_fen_index != m_fen_history.size() - 1)
      return;
  }

  if (event.type == sf::Event::KeyPressed) {
    if (event.key.code == sf::Keyboard::Left) {
      if (m_fen_index > 0) {
        m_game_view.setBoardFen(m_fen_history[m_fen_index]);
        const MoveView &info = m_move_history[m_fen_index - 1];
        core::Square epVictim = core::NO_SQUARE;
        if (info.move.isEnPassant) {
          epVictim = (info.moverColor == core::Color::White)
                         ? static_cast<core::Square>(info.move.to - 8)
                         : static_cast<core::Square>(info.move.to + 8);
        }
        m_game_view.animationMovePiece(
            info.move.to, info.move.from, core::NO_SQUARE,
            core::PieceType::None, [this, info, epVictim]() {
              if (info.move.isCapture) {
                core::Square capSq =
                    info.move.isEnPassant ? epVictim : info.move.to;
                m_game_view.addPiece(info.capturedType, ~info.moverColor,
                                     capSq);
              }
              if (info.move.promotion != core::PieceType::None) {
                m_game_view.removePiece(info.move.from);
                m_game_view.addPiece(core::PieceType::Pawn, info.moverColor,
                                     info.move.from);
              }
            });
        if (info.move.castle != model::CastleSide::None) {
          const core::Square rookFrom =
              m_chess_game.getRookSquareFromCastleside(info.move.castle,
                                                       info.moverColor);
          const core::Square rookTo =
              (info.move.castle == model::CastleSide::KingSide)
                  ? static_cast<core::Square>(info.move.to - 1)
                  : static_cast<core::Square>(info.move.to + 1);
          m_game_view.animationMovePiece(rookTo, rookFrom);
        }
        --m_fen_index;
        m_game_view.selectMove(m_fen_index ? m_fen_index - 1
                                           : static_cast<std::size_t>(-1));
        m_last_move_squares = {info.move.from, info.move.to};
        m_game_view.clearAllHighlights();
        highlightLastMove();
        m_sound_manager.playEffect(info.sound);
      }
      return;
    } else if (event.key.code == sf::Keyboard::Right) {
      if (m_fen_index < m_move_history.size()) {
        m_game_view.setBoardFen(m_fen_history[m_fen_index]);
        const MoveView &info = m_move_history[m_fen_index];
        core::Square epVictim = core::NO_SQUARE;
        if (info.move.isEnPassant) {
          epVictim = (info.moverColor == core::Color::White)
                         ? static_cast<core::Square>(info.move.to - 8)
                         : static_cast<core::Square>(info.move.to + 8);
          m_game_view.removePiece(epVictim);
        } else if (info.move.isCapture) {
          m_game_view.removePiece(info.move.to);
        }
        if (info.move.castle != model::CastleSide::None) {
          const core::Square rookFrom =
              m_chess_game.getRookSquareFromCastleside(info.move.castle,
                                                       info.moverColor);
          const core::Square rookTo =
              (info.move.castle == model::CastleSide::KingSide)
                  ? static_cast<core::Square>(info.move.to - 1)
                  : static_cast<core::Square>(info.move.to + 1);
          m_game_view.animationMovePiece(rookFrom, rookTo);
        }
        m_game_view.animationMovePiece(info.move.from, info.move.to, epVictim,
                                       info.move.promotion);
        ++m_fen_index;
        m_game_view.selectMove(m_fen_index ? m_fen_index - 1
                                           : static_cast<std::size_t>(-1));
        m_last_move_squares = {info.move.from, info.move.to};
        m_game_view.clearAllHighlights();
        highlightLastMove();
        m_sound_manager.playEffect(info.sound);
      }
      return;
    }
  }
  if (m_fen_index != m_fen_history.size() - 1)
    return;

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
  m_selection_changed_on_press = false;

  // Neue Interaktion verwirft angezeigte Premoves
  if (m_premove_from != core::NO_SQUARE)
    clearPremove();

  if (m_game_view.hasPieceOnSquare(sq)) {
    if (!tryMove(m_selected_sq, sq))
      m_game_view.setHandClosedCursor();
  } else {
    m_game_view.setDefaultCursor();
  }

  if (!m_game_view.hasPieceOnSquare(sq))
    return;

  const bool selectionWasDifferent = (m_selected_sq != sq);

  if (m_selected_sq != core::NO_SQUARE && m_selected_sq != sq) {
    m_preview_active = true;
    m_prev_selected_before_preview = m_selected_sq;

    if (!tryMove(m_selected_sq, sq)) {
      m_game_view.clearAllHighlights();
      highlightLastMove();
      selectSquare(sq);
      hoverSquare(sq);
      if (isHumanPiece(sq))
        showAttacks(getAttackSquares(sq));
    }
  } else {
    m_preview_active = false;
    m_prev_selected_before_preview = core::NO_SQUARE;

    m_game_view.clearAllHighlights();
    highlightLastMove();
    selectSquare(sq);
    hoverSquare(sq);
    if (isHumanPiece(sq))
      showAttacks(getAttackSquares(sq));
  }

  if (!tryMove(m_selected_sq, sq)) {
    m_dragging = true;
    m_drag_from = sq;
    m_game_view.setPieceToMouseScreenPos(sq, pos);
    m_game_view.playPiecePlaceHolderAnimation(sq);
  }

  m_selection_changed_on_press = selectionWasDifferent && (m_selected_sq == sq);
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

void GameController::render() { m_game_view.render(); }

void GameController::update(float dt) {
  if (m_chess_game.getResult() != core::GameResult::ONGOING)
    return;

  m_game_view.update(dt);
  m_game_view.updateEval(m_eval_cp.load());
  if (m_game_manager)
    m_game_manager->update(dt);

  // --- Sicheren Auto-Move (aus Premove) nach Engine-Zug durchführen ---
  if (m_has_pending_auto_move) {
    // Ausführen nur, wenn weiterhin Mensch am Zug und Move noch legal
    const auto st = m_chess_game.getGameState();
    if (m_game_manager && m_game_manager->isHuman(st.sideToMove) &&
        hasCurrentLegalMove(m_pending_from, m_pending_to)) {
      (void)m_game_manager->requestUserMove(m_pending_from, m_pending_to,
                                            /*onClick*/ true);
    }
    m_has_pending_auto_move = false; // Flag immer zurücksetzen
    m_pending_from = m_pending_to = core::NO_SQUARE;
  }
}

void GameController::highlightLastMove() {
  if (isValid(m_last_move_squares.first))
    m_game_view.highlightSquare(m_last_move_squares.first);
  if (isValid(m_last_move_squares.second))
    m_game_view.highlightSquare(m_last_move_squares.second);
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
  if (isValid(m_hover_sq))
    m_game_view.clearHighlightHoverSquare(m_hover_sq);
  m_hover_sq = core::NO_SQUARE;
}

void GameController::clearPremove() {
  if (m_premove_from != core::NO_SQUARE) {
    m_premove_from = core::NO_SQUARE;
    m_premove_to = core::NO_SQUARE;
    m_game_view.clearAllHighlights();
    highlightLastMove();
  }
}

// --------- ZENTRALE Move-Callback-Behandlung (auch Engine-Züge) ----------
void GameController::movePieceAndClear(const model::Move &move,
                                       bool isPlayerMove, bool onClick) {
  const core::Square from = move.from;
  const core::Square to = move.to;

  // 1) Drag-Konflikt defensiv auflösen
  if (m_dragging && m_drag_from == from) {
    m_dragging = false;
    m_mouse_down = false;
    dehoverSquare();

    m_game_view.setPieceToSquareScreenPos(from, from);
    m_game_view.endAnimation(from);
  }

  // 2) Auswahl-Konflikte entschärfen
  if (m_selected_sq == from || m_selected_sq == to) {
    deselectSquare();
  }
  m_preview_active = false;
  m_prev_selected_before_preview = core::NO_SQUARE;

  // 3) En-passant Opferfeld (für Animation)
  core::Square epVictimSq = core::NO_SQUARE;
  const core::Color moverColorBefore = ~m_chess_game.getGameState().sideToMove;
  if (move.isEnPassant) {
    epVictimSq = (moverColorBefore == core::Color::White)
                     ? static_cast<core::Square>(to - 8)
                     : static_cast<core::Square>(to + 8);
  }

  core::PieceType capturedType = core::PieceType::None;
  if (move.isCapture) {
    core::Square capSq = move.isEnPassant ? epVictimSq : to;
    capturedType = m_game_view.getPieceType(capSq);
  }

  // 4) Animation (Klick vs. Drop)
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

  view::sound::Effect effect;
  if (m_chess_game.isKingInCheck(sideToMoveNow)) {
    effect = view::sound::Effect::Check;
  } else if (move.promotion != core::PieceType::None) {
    effect = view::sound::Effect::Promotion;
  } else if (move.isCapture) {
    effect = view::sound::Effect::Capture;
  } else if (move.castle != model::CastleSide::None) {
    effect = view::sound::Effect::Castle;
  } else {
    effect = isPlayerMove ? view::sound::Effect::PlayerMove
                          : view::sound::Effect::EnemyMove;
  }

  m_sound_manager.playEffect(effect);
  m_move_history.push_back({move, moverColorBefore, capturedType, effect});

  // 7) Sichere Premove-Verarbeitung:
  //    Statt direkt im Callback zu moven (Re-Entrancy!), prüfen wir Legalität
  //    und schedulen den Auto-Move für update().
  if (m_premove_from != core::NO_SQUARE && m_game_manager &&
      m_game_manager->isHuman(sideToMoveNow)) {
    core::Square pf = m_premove_from;
    core::Square pt = m_premove_to;

    // Immer zuerst Premove-Highlights entfernen
    clearPremove();

    // Nur vormerken, wenn jetzt legal
    if (hasCurrentLegalMove(pf, pt)) {
      m_has_pending_auto_move = true;
      m_pending_from = pf;
      m_pending_to = pt;
    }
  }
}

// ------------------------------------------------------------------------

void GameController::snapAndReturn(core::Square sq, core::MousePos cur) {
  selectSquare(sq);
  m_game_view.animationSnapAndReturn(sq, cur);
}

[[nodiscard]] bool GameController::tryMove(core::Square a, core::Square b) {
  // Nur Züge für menschlich kontrollierte Figuren in Betracht ziehen
  if (!isHumanPiece(a))
    return false;
  for (auto att : getAttackSquares(a)) {
    if (att == b)
      return true;
  }
  return false;
}

[[nodiscard]] bool GameController::isPromotion(core::Square a, core::Square b) {
  for (const auto &m : m_chess_game.generateLegalMoves()) {
    if (m.from == a && m.to == b && m.promotion != core::PieceType::None)
      return true;
  }
  return false;
}

[[nodiscard]] bool GameController::isSameColor(core::Square a, core::Square b) {
  return m_game_view.isSameColorPiece(a, b);
}

[[nodiscard]] std::vector<core::Square>
GameController::getAttackSquares(core::Square pieceSQ) const {
  std::vector<core::Square> att;
  if (!isValid(pieceSQ))
    return att;

  auto pc = m_chess_game.getPiece(pieceSQ);
  if (pc.type == core::PieceType::None)
    return att;

  // Visualisierung immer aus Sicht der Figurenfarbe – unabhängig vom Zugrecht.
  model::Position pos = m_chess_game.getPositionRefForBot();
  model::GameState st = pos.getState();
  st.sideToMove = pc.color;

  model::MoveGenerator gen;
  std::vector<model::Move> pseudo;
  gen.generatePseudoLegalMoves(pos.getBoard(), st, pseudo);

  for (const auto &m : pseudo) {
    if (m.from == pieceSQ)
      att.push_back(m.to);
  }
  return att;
}

void GameController::showAttacks(std::vector<core::Square> att) {
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
  // piece might have been moved on mouse press without dragging - reset it
  if (m_game_view.hasPieceOnSquare(sq)) {
    m_game_view.endAnimation(sq);
    m_game_view.setPieceToSquareScreenPos(sq, sq);
  }

  if (m_selection_changed_on_press && sq == m_selected_sq) {
    m_selection_changed_on_press = false;
    return;
  }
  m_selection_changed_on_press = false;

  // Promotion-Auswahl?
  if (m_game_view.isInPromotionSelection()) {
    const core::PieceType promoType =
        m_game_view.getSelectedPromotion(mousePos);
    m_game_view.removePromotionSelection();
    if (m_game_manager)
      m_game_manager->completePendingPromotion(promoType);
    deselectSquare();
    return;
  }

  // Bereits etwas selektiert? -> erst Zug versuchen (hat Vorrang)
  if (m_selected_sq != core::NO_SQUARE) {
    const auto st = m_chess_game.getGameState();
    const bool ownTurnAndPiece =
        (st.sideToMove == m_chess_game.getPiece(m_selected_sq).color) &&
        (!m_game_manager || m_game_manager->isHuman(st.sideToMove));
    const core::Color humanColor = ~st.sideToMove;

    if (tryMove(m_selected_sq, sq)) {
      if (ownTurnAndPiece) {
        if (m_game_manager) {
          (void)m_game_manager->requestUserMove(m_selected_sq, sq,
                                                /*onClick*/ true);
        }
      } else if (m_chess_game.getPiece(m_selected_sq).color == humanColor &&
                 (!m_game_manager || m_game_manager->isHuman(humanColor))) {
        // Premove per Click
        m_premove_from = m_selected_sq;
        m_premove_to = sq;
        m_game_view.clearAllHighlights();
        highlightLastMove();
        m_game_view.highlightSquare(m_premove_from);
        if (m_game_view.hasPieceOnSquare(sq))
          m_game_view.highlightCaptureSquare(sq);
        else
          m_game_view.highlightAttackSquare(sq);
      }
      m_selected_sq = core::NO_SQUARE;
      return; // NICHT umselektieren
    }

    // Kein legaler Klick-Zug -> ggf. Auswahl ändern/entfernen
    if (m_game_view.hasPieceOnSquare(sq)) {
      if (sq == m_selected_sq) {
        deselectSquare();
      } else {
        m_game_view.clearAllHighlights();
        highlightLastMove();
        selectSquare(sq);
        if (isHumanPiece(sq))
          showAttacks(getAttackSquares(sq));
      }
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
    if (isHumanPiece(sq))
      showAttacks(getAttackSquares(sq));
  }
}

void GameController::onDrag(core::MousePos start, core::MousePos current) {
  const core::Square sqStart = m_game_view.mousePosToSquare(start);
  const core::Square sqMous = m_game_view.mousePosToSquare(current);

  if (m_game_view.isInPromotionSelection())
    return;
  if (!m_game_view.hasPieceOnSquare(sqStart))
    return;
  if (!m_dragging)
    return;

  // Sicherstellen, dass die Startfigur selektiert ist
  if (m_selected_sq != sqStart) {
    m_game_view.clearAllHighlights();
    highlightLastMove();
    selectSquare(sqStart);
    if (isHumanPiece(sqStart))
      showAttacks(getAttackSquares(sqStart));
  }

  if (m_hover_sq != sqMous)
    dehoverSquare();
  hoverSquare(sqMous);

  m_game_view.setPieceToMouseScreenPos(sqStart, current);
  m_game_view.playPiecePlaceHolderAnimation(sqStart);
}

void GameController::onDrop(core::MousePos start, core::MousePos end) {
  const core::Square from = m_game_view.mousePosToSquare(start);
  const core::Square to = m_game_view.mousePosToSquare(end);

  dehoverSquare();

  if (m_game_view.isInPromotionSelection())
    return;

  if (!m_game_view.hasPieceOnSquare(from)) {
    deselectSquare();
    m_preview_active = false;
    m_prev_selected_before_preview = core::NO_SQUARE;
    return;
  }

  // Platzhalter/Drag-Anim beenden, bevor wir irgendwas Neues tun
  m_game_view.endAnimation(from);

  bool accepted = false;
  bool setPremove = false;

  const auto st = m_chess_game.getGameState();
  const core::Color fromColor = m_chess_game.getPiece(from).color;
  const bool humanTurnNow =
      (m_game_manager && m_game_manager->isHuman(st.sideToMove));
  const bool movingOwnTurnPiece = humanTurnNow && (fromColor == st.sideToMove);
  const core::Color humanNextColor = ~st.sideToMove;
  const bool humanNextIsHuman =
      (!m_game_manager || m_game_manager->isHuman(humanNextColor));

  if (from != to && tryMove(from, to)) {
    if (movingOwnTurnPiece) {
      if (m_game_manager) {
        accepted = m_game_manager->requestUserMove(from, to, /*onClick*/ false);
      }
    } else if (fromColor == humanNextColor && humanNextIsHuman) {
      // Premove per Drag ablegen, wenn NICHT am Zug
      m_premove_from = from;
      m_premove_to = to;
      setPremove = true;
      m_game_view.clearAllHighlights();
      highlightLastMove();
      m_game_view.highlightSquare(m_premove_from);
      if (m_game_view.hasPieceOnSquare(to))
        m_game_view.highlightCaptureSquare(to);
      else
        m_game_view.highlightAttackSquare(to);
    }
  }

  if (!accepted) {
    // Figur visuell zurücksetzen
    m_game_view.setPieceToSquareScreenPos(from, from);

    if (!setPremove) {
      // Fehlversuch -> zurückschnappen + evtl. Warnung
      if (m_chess_game.isKingInCheck(m_chess_game.getGameState().sideToMove) &&
          m_game_manager &&
          m_game_manager->isHuman(m_chess_game.getGameState().sideToMove) &&
          from != to && m_game_view.hasPieceOnSquare(from) &&
          m_chess_game.getPiece(from).color ==
              m_chess_game.getGameState().sideToMove) {
        m_game_view.warningKingSquareAnim(
            m_chess_game.getKingSquare(m_chess_game.getGameState().sideToMove));
        m_sound_manager.playEffect(view::sound::Effect::Warning);
      }

      m_game_view.animationSnapAndReturn(from, end);

      if (m_preview_active && isValid(m_prev_selected_before_preview) &&
          m_prev_selected_before_preview != from) {
        m_game_view.clearAllHighlights();
        highlightLastMove();
        selectSquare(m_prev_selected_before_preview);
        if (isHumanPiece(m_prev_selected_before_preview))
          showAttacks(getAttackSquares(m_prev_selected_before_preview));
      } else {
        m_game_view.clearAllHighlights();
        highlightLastMove();
        selectSquare(from);
        if (isHumanPiece(from))
          showAttacks(getAttackSquares(from));
      }
    } else {
      // Bei Premove keine Snap-Animation – wir zeigen bereits die
      // Premove-Highlights.
      m_selected_sq = core::NO_SQUARE;
    }
  }

  // Preview immer aufräumen
  m_preview_active = false;
  m_prev_selected_before_preview = core::NO_SQUARE;
}

/* -------------------- Hilfsfunktionen -------------------- */

bool GameController::isHumanPiece(core::Square sq) const {
  if (!isValid(sq))
    return false;
  auto pc = m_chess_game.getPiece(sq);
  if (pc.type == core::PieceType::None)
    return false;
  return (!m_game_manager) ? true : m_game_manager->isHuman(pc.color);
}

bool GameController::hasCurrentLegalMove(core::Square from,
                                         core::Square to) const {
  if (!isValid(from) || !isValid(to))
    return false;
  // Muss zur Seite gehören, die am Zug ist
  const auto st = m_chess_game.getGameState();
  auto pc = m_chess_game.getPiece(from);
  if (pc.type == core::PieceType::None || pc.color != st.sideToMove)
    return false;

  for (const auto &m : m_chess_game.generateLegalMoves()) {
    if (m.from == from && m.to == to)
      return true;
  }
  return false;
}

void GameController::showGameOver(core::GameResult res,
                                  core::Color sideToMove) {
  std::cout << "Game is Over!" << std::endl;
  switch (res) {
  case core::GameResult::CHECKMATE:
    std::cout << "CHECKMATE -> "
              << (sideToMove == core::Color::White ? "Black won" : "White won");
    break;
  case core::GameResult::REPETITION:
    std::cout << "REPITITION -> Draw!";
    break;
  case core::GameResult::MOVERULE:
    std::cout << "MOVERULE-> Draw!";
    break;
  case core::GameResult::STALEMATE:
    std::cout << "STALEMATE -> Draw!";
    break;
  default:
    std::cout << "result is not correct";
  }
  std::cout << std::endl;
}

} // namespace lilia::controller
