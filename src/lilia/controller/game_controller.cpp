#include "lilia/controller/game_controller.hpp"

#include <SFML/System/Sleep.hpp>
#include <SFML/System/Time.hpp>
#include <iostream>
#include <string>

namespace lilia::controller {

GameController::GameController(view::GameView& gView, model::ChessGame& game)
    : m_game_view(gView), m_chess_game(game) {
  m_input_manager.setOnClick([this](core::MousePos pos) { this->onClick(pos); });

  m_input_manager.setOnDrag(
      [this](core::MousePos start, core::MousePos current) { this->onDrag(start, current); });

  m_input_manager.setOnDrop(
      [this](core::MousePos start, core::MousePos end) { this->onDrop(start, end); });

  m_sound_manager.loadSounds();
  // ------- GameManager initialisieren -------
  m_game_manager = std::make_unique<GameManager>(game);

  // Callback: wenn GameManager einen Move ausgeführt hat -> Animation & Sound
  m_game_manager->setOnMoveExecuted([this](const model::Move& mv, bool isPlayerMove, bool onClick) {
    // GameManager hat das Model bereits aktualisiert -> wir animieren & spielen Sound
    this->movePieceAndClear(mv, isPlayerMove, onClick);
    this->m_chess_game.checkGameResult();
  });

  // Callback: Promotion UI anstoßen
  m_game_manager->setOnPromotionRequested([this](core::Square sq) {
    this->m_game_view.playPromotionSelectAnim(sq, m_chess_game.getGameState().sideToMove);
  });

  // Callback: Spielende
  m_game_manager->setOnGameEnd([this](core::GameResult res) {
    // Annahme: GameView hat eine passende Anzeige-Methode (ansonsten anpassen).
    this->m_game_view.showGameOver(res, m_chess_game.getGameState().sideToMove);
    this->m_sound_manager.playGameEnds();
  });
}

void GameController::startGame(core::Color playerColor, const std::string& fen, bool vsBot) {
  m_sound_manager.playGameBegins();
  m_game_view.init(fen);
  m_game_manager->startGame(playerColor, fen, vsBot);
}

void GameController::handleEvent(const sf::Event& event) {
  if (m_chess_game.getResult() == core::GameResult::ONGOING) m_input_manager.processEvent(event);
}

void GameController::render() {
  m_game_view.render();
}

void GameController::update(float dt) {
  if (m_chess_game.getResult() == core::GameResult::ONGOING) {
    m_game_view.update(dt);
    if (m_game_manager) m_game_manager->update(dt);  // Poll bot futures & lifecycle
  }
}

void GameController::highlightLastMove() {
  if (m_last_move_squares.first != core::NO_SQUARE)
    m_game_view.highlightSquare(m_last_move_squares.first);
  if (m_last_move_squares.second != core::NO_SQUARE)
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
  if (m_hover_sq != core::NO_SQUARE) m_game_view.clearHighlightHoverSquare(m_hover_sq);
  m_hover_sq = core::NO_SQUARE;
}

void GameController::movePieceAndClear(const model::Move& move, bool isPlayerMove, bool onClick) {
  core::Square from = move.from;
  core::Square to = move.to;

  core::Color sideToTurnBeforeMove = ~m_chess_game.getGameState().sideToMove;
  core::Color sideToTurnAfterMove = m_chess_game.getGameState().sideToMove;

  core::Square dEnPassantSquare = core::NO_SQUARE;
  if (move.isEnPassant) {
    if (sideToTurnBeforeMove == core::Color::White)
      dEnPassantSquare = to - 8;
    else
      dEnPassantSquare = to + 8;
  }

  // move animation
  if (onClick)
    m_game_view.animationMovePiece(from, to, dEnPassantSquare, move.promotion);
  else
    m_game_view.animationDropPiece(from, to, dEnPassantSquare, move.promotion);

  // castling rook animation (falls relevant)
  if (move.castle != model::CastleSide::None) {
    core::Square rookSquare =
        m_chess_game.getRookSquareFromCastleside(move.castle, sideToTurnBeforeMove);
    core::Square newRookSquare;
    if (move.castle == model::CastleSide::KingSide)
      newRookSquare = to - 1;
    else
      newRookSquare = to + 1;
    m_game_view.animationMovePiece(rookSquare, newRookSquare);
  }

  // visual highlight
  m_last_move_squares = {from, to};
  deselectSquare();
  highlightLastMove();

  // Sound check: Model wurde bereits aktualisiert -> wir können aktuelle Checks abfragen
  if (m_chess_game.isKingInCheck(sideToTurnAfterMove)) {
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
  for (auto m : m_chess_game.generateLegalMoves()) {
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
  for (auto m : m_chess_game.generateLegalMoves()) {
    if (m.from == pieceSQ) att.push_back(m.to);
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
  core::Square sq = m_game_view.mousePosToSquare(mousePos);

  if (m_game_view.isInPromotionSelection()) {
    core::PieceType promoType = m_game_view.getSelectedPromotion(mousePos);
    m_game_view.removePromotionSelection();
    if (m_game_manager) m_game_manager->completePendingPromotion(promoType);
    deselectSquare();
    return;
  }

  // Keine Auswahl
  if (m_selected_sq == core::NO_SQUARE) {
    if (m_game_view.hasPieceOnSquare(sq) &&
        m_chess_game.getPiece(sq).color == m_chess_game.getGameState().sideToMove) {
      snapAndReturn(sq, mousePos);
      showAttacks(getAttackSquares(sq));
    }
    return;
  }

  // Gleiche Figur angeklickt → deselect
  if (m_selected_sq == sq) {
    snapAndReturn(sq, mousePos);
    deselectSquare();
    return;
  }

  // Versuch eines Zugs: an GameManager delegieren
  if (tryMove(m_selected_sq, sq)) {
    // GameManager kümmert sich um Promotion event bzw. Anwendung des Moves
    if (m_game_manager) {
      bool accepted = m_game_manager->requestUserMove(m_selected_sq, sq, true);
      if (!accepted) {
        // Entweder Promotion angefragt (GameManager wird callback feuern) oder illegal -> deselect
        // Wir vermeiden hier doppelte Warns; View/Manager kümmert sich um UI-Feedback
        deselectSquare();
      }
    }
  } else {
    deselectSquare();
    if (m_game_view.hasPieceOnSquare(sq) && isSameColor(m_selected_sq, sq)) {
      snapAndReturn(sq, mousePos);
      showAttacks(getAttackSquares(sq));
    }
  }
}

void GameController::onDrag(core::MousePos start, core::MousePos current) {
  core::Square sqStart = m_game_view.mousePosToSquare(start);
  core::Square sqMous = m_game_view.mousePosToSquare(current);

  if (m_game_view.isInPromotionSelection()) {
    m_game_view.removePromotionSelection();
    if (m_game_manager) m_game_manager->completePendingPromotion(core::PieceType::None);
    deselectSquare();
    return;
  }

  if (!m_game_view.hasPieceOnSquare(sqStart) ||
      m_chess_game.getPiece(sqStart).color != m_chess_game.getGameState().sideToMove)
    return;

  if (m_selected_sq != sqStart) {
    deselectSquare();
    selectSquare(sqStart);
    showAttacks(getAttackSquares(sqStart));
  }

  if (m_hover_sq != sqMous) dehoverSquare();

  hoverSquare(sqMous);

    m_game_view.setPieceToMouseScreenPos(sqStart, current);
    m_game_view.playPiecePlaceHolderAnimation(sqStart);
}

void GameController::onDrop(core::MousePos start, core::MousePos end) {
  core::Square from = m_game_view.mousePosToSquare(start);
  core::Square to = m_game_view.mousePosToSquare(end);

  dehoverSquare();

  if (m_game_view.isInPromotionSelection()) {
    m_game_view.removePromotionSelection();
    if (m_game_manager) m_game_manager->completePendingPromotion(core::PieceType::None);
    deselectSquare();
    return;
  }

  if (!m_game_view.hasPieceOnSquare(from) ||
      m_chess_game.getPiece(from).color != m_chess_game.getGameState().sideToMove) {
    deselectSquare();
    return;
  }
  m_game_view.endAnimation(from);

  if (from != to && tryMove(from, to)) {
    if (m_game_manager) {
      bool accepted = m_game_manager->requestUserMove(from, to, false);
      if (!accepted) {
        deselectSquare();
      }
    }
  } else {
    if (m_chess_game.isKingInCheck(m_chess_game.getGameState().sideToMove) && from != to) {
      m_game_view.warningKingSquareAnim(
          m_chess_game.getKingSquare(m_chess_game.getGameState().sideToMove));
      m_sound_manager.playWarning();
    }
    m_game_view.setPieceToSquareScreenPos(from, from);
    selectSquare(from);
  }
}

}  // namespace lilia::controller
