#include "lilia/controller/game_controller.hpp"

#include <SFML/System/Sleep.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/Window/Clipboard.hpp>
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
inline bool isValid(core::Square sq) {
  return sq != core::NO_SQUARE;
}

inline std::string resultToString(core::GameResult res, core::Color sideToMove) {
  switch (res) {
    case core::GameResult::CHECKMATE:
    case core::GameResult::TIMEOUT:
      return (sideToMove == core::Color::White) ? "0-1" : "1-0";
    case core::GameResult::REPETITION:
    case core::GameResult::MOVERULE:
    case core::GameResult::STALEMATE:
    case core::GameResult::INSUFFICIENT:
      return "1/2-1/2";
    default:
      return "";
  }
}

// chess.com allows multiple queued safe premoves; keep a sane limit
constexpr std::size_t MAX_PREMOVES = 8;

}  // namespace

GameController::GameController(view::GameView &gView, model::ChessGame &game)
    : m_game_view(gView), m_chess_game(game) {
  m_input_manager.setOnClick([this](core::MousePos pos) { this->onClick(pos); });
  m_input_manager.setOnDrag(
      [this](core::MousePos start, core::MousePos current) { this->onDrag(start, current); });
  m_input_manager.setOnDrop(
      [this](core::MousePos start, core::MousePos end) { this->onDrop(start, end); });

  m_sound_manager.loadSounds();

  m_game_manager = std::make_unique<GameManager>(game);
  BotPlayer::setEvalCallback([this](int eval) { m_eval_cp.store(eval); });

  m_game_manager->setOnMoveExecuted([this](const model::Move &mv, bool isPlayerMove, bool onClick) {
    // If the user is viewing history, jump back to head before applying
    if (this->m_fen_index != this->m_fen_history.size() - 1) {
      this->m_fen_index = this->m_fen_history.size() - 1;
      this->m_game_view.setBoardFen(this->m_fen_history[this->m_fen_index]);
      this->m_eval_cp.store(this->m_eval_history[this->m_fen_index]);
      this->m_game_view.updateEval(this->m_eval_history[this->m_fen_index]);
      this->m_game_view.selectMove(this->m_fen_index ? this->m_fen_index - 1
                                                     : static_cast<std::size_t>(-1));
      this->m_game_view.clearAllHighlights();
      if (!this->m_move_history.empty()) {
        const MoveView &info = this->m_move_history.back();
        this->m_last_move_squares = {info.move.from, info.move.to};
        this->highlightLastMove();
      }
      this->syncCapturedPieces();
      if (this->m_fen_index < this->m_time_history.size()) {
        const TimeView &tv = this->m_time_history[this->m_fen_index];
        this->m_game_view.updateClock(core::Color::White, tv.white);
        this->m_game_view.updateClock(core::Color::Black, tv.black);
        this->m_game_view.setClockActive(tv.active);
      }
    }

    this->movePieceAndClear(mv, isPlayerMove, onClick);
    this->m_chess_game.checkGameResult();
    this->m_game_view.addMove(move_to_uci(mv));
    this->m_fen_history.push_back(this->m_chess_game.getFen());
    this->m_eval_history.push_back(this->m_eval_cp.load());
    this->m_fen_index = this->m_fen_history.size() - 1;
    this->m_game_view.updateFen(this->m_fen_history.back());
    this->m_game_view.selectMove(this->m_fen_index ? this->m_fen_index - 1
                                                   : static_cast<std::size_t>(-1));
    core::Color stm = this->m_chess_game.getGameState().sideToMove;
    if (this->m_time_controller) {
      core::Color mover = ~stm;
      this->m_time_controller->onMove(mover);
      float w = this->m_time_controller->getTime(core::Color::White);
      float b = this->m_time_controller->getTime(core::Color::Black);
      this->m_game_view.updateClock(core::Color::White, w);
      this->m_game_view.updateClock(core::Color::Black, b);
      this->m_game_view.setClockActive(this->m_time_controller->getActive());
      this->m_time_history.push_back({w, b, stm});
    } else {
      this->m_time_history.push_back({0.f, 0.f, stm});
    }
  });

  m_game_manager->setOnPromotionRequested([this](core::Square sq) {
    this->m_game_view.playPromotionSelectAnim(sq, m_chess_game.getGameState().sideToMove);
  });

  m_game_manager->setOnGameEnd([this](core::GameResult res) {
    this->showGameOver(res, m_chess_game.getGameState().sideToMove);
  });
}

GameController::~GameController() = default;

void GameController::startGame(const std::string &fen, bool whiteIsBot, bool blackIsBot,
                               int whiteThinkTimeMs, int whiteDepth, int blackThinkTimeMs,
                               int blackDepth, bool useTimer, int baseSeconds,
                               int incrementSeconds) {
  m_sound_manager.playEffect(view::sound::Effect::GameBegins);
  m_game_view.hideResignPopup();
  m_game_view.hideGameOverPopup();
  m_game_view.setGameOver(false);
  m_game_view.init(fen);
  m_game_view.setBotMode(whiteIsBot || blackIsBot);
  m_game_manager->startGame(fen, whiteIsBot, blackIsBot, whiteThinkTimeMs, whiteDepth,
                            blackThinkTimeMs, blackDepth);

  if (useTimer) {
    m_time_controller = std::make_unique<TimeController>(baseSeconds, incrementSeconds);
    core::Color stm = m_chess_game.getGameState().sideToMove;
    m_time_controller->start(stm);
    m_game_view.setClocksVisible(true);
    m_game_view.updateClock(core::Color::White, static_cast<float>(baseSeconds));
    m_game_view.updateClock(core::Color::Black, static_cast<float>(baseSeconds));
    m_game_view.setClockActive(m_time_controller->getActive());
    m_time_history.clear();
    m_time_history.push_back(
        {static_cast<float>(baseSeconds), static_cast<float>(baseSeconds), stm});
  } else {
    m_time_controller.reset();
    m_game_view.setClocksVisible(false);
    m_time_history.clear();
    m_time_history.push_back({0.f, 0.f, m_chess_game.getGameState().sideToMove});
  }

  m_fen_history.clear();
  m_eval_history.clear();
  m_fen_history.push_back(fen);
  m_eval_history.push_back(m_eval_cp.load());
  m_fen_index = 0;
  m_move_history.clear();
  m_game_view.selectMove(static_cast<std::size_t>(-1));
  m_eval_cp.store(m_eval_history[0]);
  m_game_view.updateEval(m_eval_history[0]);
  m_game_view.clearCapturedPieces();

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
  m_premove_queue.clear();
  m_has_pending_auto_move = false;
  m_pending_from = core::NO_SQUARE;
  m_pending_to = core::NO_SQUARE;
  m_pending_capture_type = core::PieceType::None;
  m_skip_next_move_animation = false;

  m_game_view.setDefaultCursor();
  m_next_action = NextAction::None;
}

void GameController::handleEvent(const sf::Event &event) {
  // Block all input while a modal popup is open
  if (m_game_view.isResignPopupOpen() || m_game_view.isGameOverPopupOpen()) {
    m_mouse_down = false;
    m_dragging = false;
    m_game_view.setDefaultCursor();

    if (event.type == sf::Event::MouseButtonPressed &&
        event.mouseButton.button == sf::Mouse::Left) {
      core::MousePos mp(event.mouseButton.x, event.mouseButton.y);

      if (m_game_view.isResignPopupOpen()) {
        if (m_game_view.isOnResignYes(mp)) {
          resign();
        }
        m_game_view.hideResignPopup();
      } else if (m_game_view.isGameOverPopupOpen()) {
        if (m_game_view.isOnNewBot(mp)) {
          m_next_action = NextAction::NewBot;
        } else if (m_game_view.isOnRematch(mp)) {
          m_next_action = NextAction::Rematch;
        }
        m_game_view.hideGameOverPopup();
      }
    }
    return;
  }

  if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
    core::MousePos mp(event.mouseButton.x, event.mouseButton.y);

    if (m_game_view.isOnEvalToggle(mp)) {
      m_game_view.toggleEvalBarVisibility();
      return;
    }
    if (m_game_view.isOnFlipIcon(mp)) {
      m_game_view.toggleBoardOrientation();
      return;
    }

    auto opt = m_game_view.getOptionAt(mp);
    switch (opt) {
      case view::MoveListView::Option::Resign:
        m_game_view.showResignPopup();
        m_mouse_down = false;
        m_dragging = false;
        m_game_view.setDefaultCursor();
        return;
      case view::MoveListView::Option::Prev:
        stepBackward();
        return;
      case view::MoveListView::Option::Next:
        stepForward();
        return;
      case view::MoveListView::Option::Settings:
        return;
      case view::MoveListView::Option::NewBot:
        m_next_action = NextAction::NewBot;
        return;
      case view::MoveListView::Option::Rematch:
        m_next_action = NextAction::Rematch;
        return;
      case view::MoveListView::Option::ShowFen:
        sf::Clipboard::setString(m_fen_history[m_fen_index]);
        return;
      default:
        break;
    }

    std::size_t idx =
        m_game_view.getMoveIndexAt(core::MousePos(event.mouseButton.x, event.mouseButton.y));
    if (idx != static_cast<std::size_t>(-1)) {
      const bool leavingFinalState =
          (m_chess_game.getResult() != core::GameResult::ONGOING &&
           m_fen_index == m_fen_history.size() - 1 && idx + 1 != m_fen_history.size() - 1);
      const bool enteringFinalState = (m_chess_game.getResult() != core::GameResult::ONGOING &&
                                       idx + 1 == m_fen_history.size() - 1);

      if (leavingFinalState) m_game_view.resetEvalBar();

      m_fen_index = idx + 1;
      m_game_view.setBoardFen(m_fen_history[m_fen_index]);
      m_game_view.selectMove(idx);
      const MoveView &info = m_move_history[idx];
      m_last_move_squares = {info.move.from, info.move.to};
      m_game_view.clearAllHighlights();
      highlightLastMove();
      m_sound_manager.playEffect(info.sound);
      m_eval_cp.store(m_eval_history[m_fen_index]);
      m_game_view.updateEval(m_eval_history[m_fen_index]);
      if (enteringFinalState) {
        m_game_view.setEvalResult(
            resultToString(m_chess_game.getResult(), m_chess_game.getGameState().sideToMove));
      }
      if (m_fen_index < m_time_history.size()) {
        const TimeView &tv = m_time_history[m_fen_index];
        m_game_view.updateClock(core::Color::White, tv.white);
        m_game_view.updateClock(core::Color::Black, tv.black);
        bool latest = (m_fen_index == m_fen_history.size() - 1 &&
                       m_chess_game.getResult() == core::GameResult::ONGOING);
        if (latest)
          m_game_view.setClockActive(tv.active);
        else
          m_game_view.setClockActive(std::nullopt);
      }
      syncCapturedPieces();
      return;
    }
  }

  if (event.type == sf::Event::MouseWheelScrolled) {
    m_game_view.scrollMoveList(event.mouseWheelScroll.delta);
    if (m_fen_index != m_fen_history.size() - 1) return;
  }

  if (event.type == sf::Event::KeyPressed) {
    if (event.key.code == sf::Keyboard::Left) {
      stepBackward();
      return;
    }
    if (event.key.code == sf::Keyboard::Right) {
      stepForward();
      return;
    }
  }
  if (m_fen_index != m_fen_history.size() - 1) return;

  if (m_chess_game.getResult() != core::GameResult::ONGOING) {
    if (event.type == sf::Event::MouseButtonPressed &&
        event.mouseButton.button == sf::Mouse::Left) {
      core::MousePos mp(event.mouseButton.x, event.mouseButton.y);
      if (m_game_view.isOnFlipIcon(mp)) {
        m_game_view.toggleBoardOrientation();
      }
    }
    return;
  }

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
    case sf::Event::LostFocus:
    case sf::Event::MouseLeft:
      m_mouse_down = false;
      m_dragging = false;
      m_game_view.setDefaultCursor();
      break;
    default:
      break;
  }
  m_input_manager.processEvent(event);
}

/* -------------------- Mouse handling -------------------- */
void GameController::onMouseMove(core::MousePos pos) {
  if (m_dragging || m_mouse_down) {
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

  if (!hasVirtualPiece(sq)) {
    m_game_view.setDefaultCursor();
    return;
  }

  const bool selectionWasDifferent = (m_selected_sq != sq);

  if (m_selected_sq != core::NO_SQUARE && m_selected_sq != sq) {
    m_preview_active = true;
    m_prev_selected_before_preview = m_selected_sq;

    if (!tryMove(m_selected_sq, sq)) {
      m_game_view.clearNonPremoveHighlights();
      highlightLastMove();
      selectSquare(sq);
      hoverSquare(sq);
      if (isHumanPiece(sq)) showAttacks(getAttackSquares(sq));
    }
  } else {
    m_preview_active = false;
    m_prev_selected_before_preview = core::NO_SQUARE;

    m_game_view.clearNonPremoveHighlights();
    highlightLastMove();
    selectSquare(sq);
    hoverSquare(sq);
    if (isHumanPiece(sq)) showAttacks(getAttackSquares(sq));
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

/* -------------------- Main loop hooks -------------------- */
void GameController::render() {
  m_game_view.render();
}

void GameController::update(float dt) {
  // Always tick UI/animations/particles
  m_game_view.update(dt);
  m_game_view.updateEval(m_eval_cp.load());

  if (m_chess_game.getResult() != core::GameResult::ONGOING) return;

  if (m_time_controller) {
    m_time_controller->update(dt);
    if (!m_time_history.empty()) {
      m_time_history.back().white = m_time_controller->getTime(core::Color::White);
      m_time_history.back().black = m_time_controller->getTime(core::Color::Black);
    }
    if (m_fen_index == m_fen_history.size() - 1) {
      m_game_view.updateClock(core::Color::White, m_time_controller->getTime(core::Color::White));
      m_game_view.updateClock(core::Color::Black, m_time_controller->getTime(core::Color::Black));
      m_game_view.setClockActive(m_time_controller->getActive());
    } else if (m_fen_index < m_time_history.size()) {
      const TimeView &tv = m_time_history[m_fen_index];
      m_game_view.updateClock(core::Color::White, tv.white);
      m_game_view.updateClock(core::Color::Black, tv.black);
      m_game_view.setClockActive(std::nullopt);
    }
    if (auto flag = m_time_controller->getFlagged()) {
      m_chess_game.setResult(core::GameResult::TIMEOUT);
      if (m_game_manager) m_game_manager->stopGame();
      showGameOver(core::GameResult::TIMEOUT, *flag);
      return;
    }
  }

  if (m_game_manager) m_game_manager->update(dt);

  // Auto-play first premove if legal when our turn starts
  if (m_has_pending_auto_move) {
    const auto st = m_chess_game.getGameState();
    if (m_game_manager && m_game_manager->isHuman(st.sideToMove) &&
        hasCurrentLegalMove(m_pending_from, m_pending_to)) {
      // Consume only the first ghost; keep the rest visible
      m_game_view.consumePremoveGhost(m_pending_from, m_pending_to);

      // Commit instantly (no animation), then hand it to the game manager
      m_game_view.movePiece(m_pending_from, m_pending_to);
      (void)m_game_manager->requestUserMove(m_pending_from, m_pending_to, /*onClick*/ true);
    } else {
      // Cancel entire chain if first premove is no longer legal
      clearPremove();
    }
    m_has_pending_auto_move = false;
    m_pending_from = m_pending_to = core::NO_SQUARE;
    m_pending_capture_type = core::PieceType::None;
  }
}

/* -------------------- Highlights -------------------- */
void GameController::highlightLastMove() {
  if (isValid(m_last_move_squares.first)) m_game_view.highlightSquare(m_last_move_squares.first);
  if (isValid(m_last_move_squares.second)) m_game_view.highlightSquare(m_last_move_squares.second);
}

void GameController::selectSquare(core::Square sq) {
  m_game_view.highlightSquare(sq);
  m_selected_sq = sq;
}

void GameController::deselectSquare() {
  m_game_view.clearNonPremoveHighlights();
  highlightLastMove();
  m_selected_sq = core::NO_SQUARE;
}

void GameController::clearLastMoveHighlight() {
  if (isValid(m_last_move_squares.first))
    m_game_view.clearHighlightSquare(m_last_move_squares.first);
  if (isValid(m_last_move_squares.second))
    m_game_view.clearHighlightSquare(m_last_move_squares.second);
}

void GameController::hoverSquare(core::Square sq) {
  m_hover_sq = sq;
  m_game_view.highlightHoverSquare(m_hover_sq);
}

void GameController::dehoverSquare() {
  if (isValid(m_hover_sq)) m_game_view.clearHighlightHoverSquare(m_hover_sq);
  m_hover_sq = core::NO_SQUARE;
}

/* -------------------- Premove queue -------------------- */
void GameController::enqueuePremove(core::Square from, core::Square to) {
  // Only allow premove for the human side NOT to move (chess.com behavior)
  const auto st = m_chess_game.getGameState();
  if (!m_game_manager || !m_game_manager->isHuman(~st.sideToMove)) return;
  if (m_game_manager->isHuman(st.sideToMove)) return;  // don't premove on your turn

  if (m_premove_queue.size() >= MAX_PREMOVES) return;
  if (!isPseudoLegalPremove(from, to)) return;

  model::Position pos = getPositionAfterPremoves();
  Premove pm{from, to, core::PieceType::None, core::Color::White};
  if (auto cap = pos.getBoard().getPiece(to)) {
    pm.capturedType = cap->type;
    pm.capturedColor = cap->color;
  }

  // Visuals
  m_game_view.clearAttackHighlights();
  m_game_view.clearHighlightSquare(from);
  m_game_view.highlightPremoveSquare(from);
  m_game_view.highlightPremoveSquare(to);

  m_premove_queue.push_back(pm);
  m_sound_manager.playEffect(view::sound::Effect::Premove);
  updatePremovePreviews();
}

void GameController::clearPremove() {
  if (!m_premove_queue.empty()) {
    m_premove_queue.clear();
    m_game_view.clearPremoveHighlights();
    m_game_view.clearPremovePieces(true);  // restore hidden pieces
    highlightLastMove();
  }
}

void GameController::updatePremovePreviews() {
  // Restore board from any previous preview, then rebuild from queue head->tail
  m_game_view.clearPremovePieces(true);
  for (const auto &pm : m_premove_queue) {
    m_game_view.showPremovePiece(pm.from, pm.to);
  }
}

/* --------- Central move handling (both human/engine) ---------- */
void GameController::movePieceAndClear(const model::Move &move, bool isPlayerMove, bool onClick) {
  const core::Square from = move.from;
  const core::Square to = move.to;

  // 1) Resolve drag conflicts
  if (m_dragging && m_drag_from == from) {
    m_dragging = false;
    m_mouse_down = false;
    dehoverSquare();
    m_game_view.setPieceToSquareScreenPos(from, from);
    m_game_view.endAnimation(from);
  }

  // 2) Selection cleanup
  if (m_selected_sq == from || m_selected_sq == to) deselectSquare();
  m_preview_active = false;
  m_prev_selected_before_preview = core::NO_SQUARE;

  // 3) En-passant victim square for visuals
  core::Square epVictimSq = core::NO_SQUARE;
  const core::Color moverColorBefore = ~m_chess_game.getGameState().sideToMove;
  if (move.isEnPassant) {
    epVictimSq = (moverColorBefore == core::Color::White) ? static_cast<core::Square>(to - 8)
                                                          : static_cast<core::Square>(to + 8);
  }

  core::PieceType capturedType = core::PieceType::None;
  if (move.isCapture) {
    core::Square capSq = move.isEnPassant ? epVictimSq : to;
    capturedType = m_game_view.getPieceType(capSq);
    if (capturedType == core::PieceType::None && m_pending_capture_type != core::PieceType::None)
      capturedType = m_pending_capture_type;
  }

  // 4) Animate or drop
  if (!m_skip_next_move_animation) {
    if (onClick)
      m_game_view.animationMovePiece(from, to, epVictimSq, move.promotion);
    else
      m_game_view.animationDropPiece(from, to, epVictimSq, move.promotion);
  }
  m_skip_next_move_animation = false;
  m_pending_capture_type = core::PieceType::None;

  // 5) Castling rook
  if (move.castle != model::CastleSide::None) {
    const core::Square rookFrom =
        m_chess_game.getRookSquareFromCastleside(move.castle, moverColorBefore);
    const core::Square rookTo = (move.castle == model::CastleSide::KingSide)
                                    ? static_cast<core::Square>(to - 1)
                                    : static_cast<core::Square>(to + 1);
    m_game_view.animationMovePiece(rookFrom, rookTo);
  }

  // 6) Visuals / Sounds
  clearLastMoveHighlight();
  m_last_move_squares = {from, to};
  highlightLastMove();
  if (isValid(m_selected_sq)) m_game_view.highlightSquare(m_selected_sq);

  const core::Color sideToMoveNow = m_chess_game.getGameState().sideToMove;

  view::sound::Effect effect;
  if (m_chess_game.isKingInCheck(sideToMoveNow))
    effect = view::sound::Effect::Check;
  else if (move.promotion != core::PieceType::None)
    effect = view::sound::Effect::Promotion;
  else if (move.isCapture)
    effect = view::sound::Effect::Capture;
  else if (move.castle != model::CastleSide::None)
    effect = view::sound::Effect::Castle;
  else
    effect = isPlayerMove ? view::sound::Effect::PlayerMove : view::sound::Effect::EnemyMove;

  m_sound_manager.playEffect(effect);
  if (move.isCapture) m_game_view.addCapturedPiece(moverColorBefore, capturedType);
  m_move_history.push_back({move, moverColorBefore, capturedType, effect, m_eval_cp.load()});

  // 7) Safe premove processing
  if (!m_premove_queue.empty() && m_game_manager && m_game_manager->isHuman(sideToMoveNow)) {
    Premove pm = m_premove_queue.front();
    m_premove_queue.pop_front();

    if (hasCurrentLegalMove(pm.from, pm.to)) {
      m_has_pending_auto_move = true;
      m_pending_from = pm.from;
      m_pending_to = pm.to;
      m_pending_capture_type = pm.capturedType;
      m_skip_next_move_animation = true;
    } else {
      clearPremove();  // cancel entire chain on failure
    }

    m_game_view.clearPremoveHighlights();
    for (const auto &remaining : m_premove_queue) {
      m_game_view.highlightPremoveSquare(remaining.from);
      m_game_view.highlightPremoveSquare(remaining.to);
    }
    updatePremovePreviews();
  }
}

/* ------------------------------------------------------------------------ */

void GameController::snapAndReturn(core::Square sq, core::MousePos cur) {
  selectSquare(sq);
  m_game_view.animationSnapAndReturn(sq, cur);
}

[[nodiscard]] bool GameController::tryMove(core::Square a, core::Square b) {
  if (!isHumanPiece(a)) return false;
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
  if (!isValid(pieceSQ)) return att;

  model::Position pos = getPositionAfterPremoves();
  auto pcOpt = pos.getBoard().getPiece(pieceSQ);
  if (!pcOpt) return att;

  bool forPremove = (m_chess_game.getGameState().sideToMove != pcOpt->color);
  model::Board board = pos.getBoard();
  model::GameState st = pos.getState();
  st.sideToMove = pcOpt->color;
  if (forPremove) {
    // Safe premove generation (like chess.com):
    // isolate the piece; disable castling/en-passant; ignore checks
    board.clear();
    board.setPiece(pieceSQ, *pcOpt);
    st.castlingRights = 0;
    st.enPassantSquare = core::NO_SQUARE;
  }

  model::MoveGenerator gen;
  std::vector<model::Move> pseudo;
  gen.generatePseudoLegalMoves(board, st, pseudo);

  if (forPremove) {
    for (const auto &m : pseudo)
      if (m.from == pieceSQ) att.push_back(m.to);
  } else {
    for (const auto &m : pseudo) {
      if (m.from == pieceSQ && pos.doMove(m)) {
        att.push_back(m.to);
        pos.undoMove();
      }
    }
  }
  return att;
}

void GameController::showAttacks(std::vector<core::Square> att) {
  m_game_view.clearAttackHighlights();
  for (auto sq : att) {
    if (hasVirtualPiece(sq))
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

  // Promotion dialog?
  if (m_game_view.isInPromotionSelection()) {
    const core::PieceType promoType = m_game_view.getSelectedPromotion(mousePos);
    m_game_view.removePromotionSelection();
    if (m_game_manager) m_game_manager->completePendingPromotion(promoType);
    deselectSquare();
    return;
  }

  // If something is selected, try that move first
  if (m_selected_sq != core::NO_SQUARE) {
    const auto st = m_chess_game.getGameState();
    auto selPiece = getPieceConsideringPremoves(m_selected_sq);
    const bool ownTurnAndPiece =
        (selPiece.type != core::PieceType::None && st.sideToMove == selPiece.color &&
         (!m_game_manager || m_game_manager->isHuman(st.sideToMove)));
    const core::Color humanColor = ~st.sideToMove;
    const bool canPremove =
        (selPiece.type != core::PieceType::None && selPiece.color == humanColor &&
         (!m_game_manager || m_game_manager->isHuman(humanColor)));

    if (ownTurnAndPiece && tryMove(m_selected_sq, sq)) {
      if (m_game_manager) {
        (void)m_game_manager->requestUserMove(m_selected_sq, sq, /*onClick*/ true);
      }
      m_selected_sq = core::NO_SQUARE;
      return;  // don't reselect
    }
    if (!ownTurnAndPiece && canPremove) {
      enqueuePremove(m_selected_sq, sq);
      m_selected_sq = core::NO_SQUARE;
      return;  // don't reselect
    }

    // Not a legal click move -> maybe change selection
    if (hasVirtualPiece(sq)) {
      if (sq == m_selected_sq) {
        deselectSquare();
      } else {
        m_game_view.clearNonPremoveHighlights();
        highlightLastMove();
        selectSquare(sq);
        if (isHumanPiece(sq)) showAttacks(getAttackSquares(sq));
      }
    } else {
      deselectSquare();
    }
    return;
  }

  // Nothing selected yet: select if there is a (virtual) piece
  if (hasVirtualPiece(sq)) {
    m_game_view.clearNonPremoveHighlights();
    highlightLastMove();
    selectSquare(sq);
    if (isHumanPiece(sq)) showAttacks(getAttackSquares(sq));
  }
}

void GameController::onDrag(core::MousePos start, core::MousePos current) {
  const core::Square sqStart = m_game_view.mousePosToSquare(start);
  const core::MousePos clamped = m_game_view.clampPosToBoard(current);
  const core::Square sqMous = m_game_view.mousePosToSquare(clamped);

  if (m_game_view.isInPromotionSelection()) return;
  if (!hasVirtualPiece(sqStart)) return;
  if (!m_dragging) return;

  // Ensure start is selected
  if (m_selected_sq != sqStart) {
    m_game_view.clearNonPremoveHighlights();
    highlightLastMove();
    selectSquare(sqStart);
    if (isHumanPiece(sqStart)) showAttacks(getAttackSquares(sqStart));
  }

  if (m_hover_sq != sqMous) dehoverSquare();
  hoverSquare(sqMous);

  m_game_view.setPieceToMouseScreenPos(sqStart, current);
  m_game_view.playPiecePlaceHolderAnimation(sqStart);
}

void GameController::onDrop(core::MousePos start, core::MousePos end) {
  const core::Square from = m_game_view.mousePosToSquare(start);
  const core::Square to = m_game_view.mousePosToSquare(m_game_view.clampPosToBoard(end));

  dehoverSquare();

  if (m_game_view.isInPromotionSelection()) return;

  if (!hasVirtualPiece(from)) {
    deselectSquare();
    m_preview_active = false;
    m_prev_selected_before_preview = core::NO_SQUARE;
    return;
  }

  // End drag placeholder before doing anything
  m_game_view.endAnimation(from);

  bool accepted = false;
  bool setPremove = false;

  const auto st = m_chess_game.getGameState();
  const core::Color fromColor = getPieceConsideringPremoves(from).color;
  const bool humanTurnNow = (m_game_manager && m_game_manager->isHuman(st.sideToMove));
  const bool movingOwnTurnPiece = humanTurnNow && (fromColor == st.sideToMove);
  const core::Color humanNextColor = ~st.sideToMove;
  const bool humanNextIsHuman = (!m_game_manager || m_game_manager->isHuman(humanNextColor));

  if (from != to) {
    if (movingOwnTurnPiece && tryMove(from, to)) {
      if (m_game_manager) {
        accepted = m_game_manager->requestUserMove(from, to, /*onClick*/ false);
      }
    } else if (fromColor == humanNextColor && humanNextIsHuman) {
      // Drag-to-premove when it's not your turn
      enqueuePremove(from, to);
      setPremove = true;
    }
  }

  if (!accepted) {
    if (!setPremove) {
      m_game_view.setPieceToSquareScreenPos(from, from);

      // Warning snap if you're in check and tried an illegal drop
      if (m_chess_game.isKingInCheck(m_chess_game.getGameState().sideToMove) && m_game_manager &&
          m_game_manager->isHuman(m_chess_game.getGameState().sideToMove) && from != to &&
          m_game_view.hasPieceOnSquare(from) &&
          m_chess_game.getPiece(from).color == m_chess_game.getGameState().sideToMove) {
        m_game_view.warningKingSquareAnim(
            m_chess_game.getKingSquare(m_chess_game.getGameState().sideToMove));
        m_sound_manager.playEffect(view::sound::Effect::Warning);
      }

      m_game_view.animationSnapAndReturn(from, end);

      if (m_preview_active && isValid(m_prev_selected_before_preview) &&
          m_prev_selected_before_preview != from) {
        m_game_view.clearNonPremoveHighlights();
        highlightLastMove();
        selectSquare(m_prev_selected_before_preview);
        if (isHumanPiece(m_prev_selected_before_preview))
          showAttacks(getAttackSquares(m_prev_selected_before_preview));
      } else {
        m_game_view.clearNonPremoveHighlights();
        highlightLastMove();
        selectSquare(from);
        if (isHumanPiece(from)) showAttacks(getAttackSquares(from));
      }
    } else {
      // For premove, don't snap back or reselect
      m_selected_sq = core::NO_SQUARE;
    }
  }

  // Always clear preview state
  m_preview_active = false;
  m_prev_selected_before_preview = core::NO_SQUARE;
}

/* -------------------- Helpers -------------------- */
bool GameController::isHumanPiece(core::Square sq) const {
  if (!isValid(sq)) return false;
  auto pc = getPieceConsideringPremoves(sq);
  if (pc.type == core::PieceType::None) return false;
  return (!m_game_manager) ? true : m_game_manager->isHuman(pc.color);
}

bool GameController::hasCurrentLegalMove(core::Square from, core::Square to) const {
  if (!isValid(from) || !isValid(to)) return false;
  const auto st = m_chess_game.getGameState();
  auto pc = m_chess_game.getPiece(from);
  if (pc.type == core::PieceType::None || pc.color != st.sideToMove) return false;

  for (const auto &m : m_chess_game.generateLegalMoves()) {
    if (m.from == from && m.to == to) return true;
  }
  return false;
}

model::Position GameController::getPositionAfterPremoves() const {
  model::Position pos = m_chess_game.getPositionRefForBot();
  if (m_premove_queue.empty()) return pos;
  model::MoveGenerator gen;
  for (const auto &pm : m_premove_queue) {
    auto mover = pos.getBoard().getPiece(pm.from);
    if (!mover) break;
    pos.getState().sideToMove = mover->color;
    std::vector<model::Move> pseudo;
    gen.generatePseudoLegalMoves(pos.getBoard(), pos.getState(), pseudo);
    auto it = std::find_if(pseudo.begin(), pseudo.end(), [&](const model::Move &m) {
      return m.from == pm.from && m.to == pm.to;
    });
    if (it == pseudo.end()) break;
    pos.doMove(*it);
    pos.getState().sideToMove = mover->color;  // keep same color for chaining preview
  }
  return pos;
}

model::bb::Piece GameController::getPieceConsideringPremoves(core::Square sq) const {
  // Prefer the virtual board after queued premoves (fixes "captured piece steals selection")
  if (!m_premove_queue.empty()) {
    model::Position pos = getPositionAfterPremoves();
    if (auto virt = pos.getBoard().getPiece(sq)) return *virt;
  }
  return m_chess_game.getPiece(sq);
}

bool GameController::hasVirtualPiece(core::Square sq) const {
  return getPieceConsideringPremoves(sq).type != core::PieceType::None;
}

bool GameController::isPseudoLegalPremove(core::Square from, core::Square to) const {
  if (!isValid(from) || !isValid(to)) return false;
  model::Position pos = getPositionAfterPremoves();
  auto pcOpt = pos.getBoard().getPiece(from);
  if (!pcOpt) return false;

  // Safe premove: isolate moving piece, disable castle/en-passant, ignore checks
  model::Board empty;
  empty.clear();
  empty.setPiece(from, *pcOpt);
  model::GameState st{};
  st.sideToMove = pcOpt->color;
  st.castlingRights = 0;
  st.enPassantSquare = core::NO_SQUARE;

  model::MoveGenerator gen;
  std::vector<model::Move> pseudo;
  gen.generatePseudoLegalMoves(empty, st, pseudo);
  for (const auto &m : pseudo) {
    if (m.from == from && m.to == to) return true;
  }
  return false;
}

void GameController::showGameOver(core::GameResult res, core::Color sideToMove) {
  // Reset any dragging state and cursor
  m_mouse_down = false;
  m_dragging = false;
  m_game_view.setDefaultCursor();

  if (m_time_controller) {
    m_time_controller->stop();
    m_game_view.setClockActive(std::nullopt);
    if (!m_time_history.empty()) {
      const TimeView &tv = m_time_history.back();
      m_game_view.updateClock(core::Color::White, tv.white);
      m_game_view.updateClock(core::Color::Black, tv.black);
    }
  }

  m_sound_manager.playEffect(view::sound::Effect::GameEnds);
  std::string resultStr;
  switch (res) {
    case core::GameResult::CHECKMATE:
      resultStr = (sideToMove == core::Color::White) ? "0-1" : "1-0";
      m_game_view.showGameOverPopup(sideToMove == core::Color::White ? "Black won" : "White won");
      break;
    case core::GameResult::TIMEOUT:
      resultStr = (sideToMove == core::Color::White) ? "0-1" : "1-0";
      m_game_view.showGameOverPopup(sideToMove == core::Color::White ? "Black wins on time"
                                                                     : "White wins on time");
      break;
    case core::GameResult::REPETITION:
      resultStr = "1/2-1/2";
      m_game_view.showGameOverPopup("Draw by repetition");
      break;
    case core::GameResult::MOVERULE:
      resultStr = "1/2-1/2";
      m_game_view.showGameOverPopup("Draw by 50 move rule");
      break;
    case core::GameResult::STALEMATE:
      resultStr = "1/2-1/2";
      m_game_view.showGameOverPopup("Stalemate");
      break;
    case core::GameResult::INSUFFICIENT:
      resultStr = "1/2-1/2";
      m_game_view.showGameOverPopup("Insufficient material");
      break;
    default:
      resultStr = "error";
      m_game_view.showGameOverPopup("result is not correct");
      break;
  }
  m_game_view.addResult(resultStr);
  m_game_view.setGameOver(true);
}

void GameController::syncCapturedPieces() {
  m_game_view.clearCapturedPieces();
  for (std::size_t i = 0; i < m_fen_index; ++i) {
    const MoveView &mv = m_move_history[i];
    if (mv.move.isCapture) {
      m_game_view.addCapturedPiece(mv.moverColor, mv.capturedType);
    }
  }
}

void GameController::stepBackward() {
  if (m_fen_index > 0) {
    const bool leavingFinalState = (m_chess_game.getResult() != core::GameResult::ONGOING &&
                                    m_fen_index == m_fen_history.size() - 1);

    m_game_view.setBoardFen(m_fen_history[m_fen_index]);
    const MoveView &info = m_move_history[m_fen_index - 1];
    core::Square epVictim = core::NO_SQUARE;
    if (info.move.isEnPassant) {
      epVictim = (info.moverColor == core::Color::White)
                     ? static_cast<core::Square>(info.move.to - 8)
                     : static_cast<core::Square>(info.move.to + 8);
    }
    m_game_view.animationMovePiece(
        info.move.to, info.move.from, core::NO_SQUARE, core::PieceType::None,
        [this, info, epVictim]() {
          if (info.move.isCapture) {
            core::Square capSq = info.move.isEnPassant ? epVictim : info.move.to;
            m_game_view.addPiece(info.capturedType, ~info.moverColor, capSq);
          }
          if (info.move.promotion != core::PieceType::None) {
            m_game_view.removePiece(info.move.from);
            m_game_view.addPiece(core::PieceType::Pawn, info.moverColor, info.move.from);
          }
        });
    if (info.move.castle != model::CastleSide::None) {
      const core::Square rookFrom =
          m_chess_game.getRookSquareFromCastleside(info.move.castle, info.moverColor);
      const core::Square rookTo = (info.move.castle == model::CastleSide::KingSide)
                                      ? static_cast<core::Square>(info.move.to - 1)
                                      : static_cast<core::Square>(info.move.to + 1);
      m_game_view.animationMovePiece(rookTo, rookFrom);
    }
    --m_fen_index;
    m_game_view.selectMove(m_fen_index ? m_fen_index - 1 : static_cast<std::size_t>(-1));
    m_last_move_squares = {info.move.from, info.move.to};
    m_game_view.clearAllHighlights();
    highlightLastMove();
    m_sound_manager.playEffect(info.sound);
    m_eval_cp.store(m_eval_history[m_fen_index]);
    if (leavingFinalState) m_game_view.resetEvalBar();
    m_game_view.updateEval(m_eval_history[m_fen_index]);
    m_game_view.updateFen(m_fen_history[m_fen_index]);
    if (m_fen_index < m_time_history.size()) {
      const TimeView &tv = m_time_history[m_fen_index];
      m_game_view.updateClock(core::Color::White, tv.white);
      m_game_view.updateClock(core::Color::Black, tv.black);
      bool latest = (m_fen_index == m_fen_history.size() - 1 &&
                     m_chess_game.getResult() == core::GameResult::ONGOING);
      if (latest)
        m_game_view.setClockActive(tv.active);
      else
        m_game_view.setClockActive(std::nullopt);
    }
    syncCapturedPieces();
  }
}

void GameController::stepForward() {
  if (m_fen_index < m_move_history.size()) {
    const bool enteringFinalState = (m_chess_game.getResult() != core::GameResult::ONGOING &&
                                     m_fen_index + 1 == m_fen_history.size() - 1);

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
          m_chess_game.getRookSquareFromCastleside(info.move.castle, info.moverColor);
      const core::Square rookTo = (info.move.castle == model::CastleSide::KingSide)
                                      ? static_cast<core::Square>(info.move.to - 1)
                                      : static_cast<core::Square>(info.move.to + 1);
      m_game_view.animationMovePiece(rookFrom, rookTo);
    }
    m_game_view.animationMovePiece(info.move.from, info.move.to, epVictim, info.move.promotion);
    ++m_fen_index;
    m_game_view.selectMove(m_fen_index ? m_fen_index - 1 : static_cast<std::size_t>(-1));
    m_last_move_squares = {info.move.from, info.move.to};
    m_game_view.clearAllHighlights();
    highlightLastMove();
    m_sound_manager.playEffect(info.sound);
    m_eval_cp.store(m_eval_history[m_fen_index]);
    m_game_view.updateEval(m_eval_history[m_fen_index]);
    if (enteringFinalState) {
      m_game_view.setEvalResult(
          resultToString(m_chess_game.getResult(), m_chess_game.getGameState().sideToMove));
    }
    m_game_view.updateFen(m_fen_history[m_fen_index]);
    if (m_fen_index < m_time_history.size()) {
      const TimeView &tv = m_time_history[m_fen_index];
      m_game_view.updateClock(core::Color::White, tv.white);
      m_game_view.updateClock(core::Color::Black, tv.black);
      bool latest = (m_fen_index == m_fen_history.size() - 1 &&
                     m_chess_game.getResult() == core::GameResult::ONGOING);
      if (latest)
        m_game_view.setClockActive(tv.active);
      else
        m_game_view.setClockActive(std::nullopt);
    }
    syncCapturedPieces();
  }
}

void GameController::resign() {
  m_game_manager->stopGame();
  m_chess_game.setResult(core::GameResult::CHECKMATE);
  showGameOver(core::GameResult::CHECKMATE, m_chess_game.getGameState().sideToMove);
}

GameController::NextAction GameController::getNextAction() const {
  return m_next_action;
}

}  // namespace lilia::controller
