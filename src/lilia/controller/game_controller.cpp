#include "lilia/controller/game_controller.hpp"

#include <SFML/System/Sleep.hpp>
#include <SFML/System/Time.hpp>
#include <iostream>
#include <string>

namespace lilia::controller {

GameController::GameController(view::GameView& gView, model::ChessGame& game)
    : m_gameView(gView), m_chess_game(game) {
  m_inputManager.setOnClick([this](core::MousePos pos) { this->onClick(pos); });

  m_inputManager.setOnDrag(
      [this](core::MousePos start, core::MousePos current) { this->onDrag(start, current); });

  m_inputManager.setOnDrop(
      [this](core::MousePos start, core::MousePos end) { this->onDrop(start, end); });

  m_sound_manager.loadSounds();
  
  m_gameManager = std::make_unique<GameManager>(game);

  
  m_gameManager->setOnMoveExecuted([this](const model::Move& mv, bool isPlayerMove, bool onClick) {
    
    this->movePieceAndClear(mv, isPlayerMove, onClick);
    this->m_chess_game.checkGameResult();
  });

  
  m_gameManager->setOnPromotionRequested([this](core::Square sq) {
    this->m_gameView.playPromotionSelectAnim(sq, m_chess_game.getGameState().sideToMove);
  });

  
  m_gameManager->setOnGameEnd([this](core::GameResult res) {
    
    this->m_gameView.showGameOver(res, m_chess_game.getGameState().sideToMove);
    this->m_sound_manager.playGameEnds();
  });
}

void GameController::startGame(core::Color playerColor, const std::string& fen, bool vsBot) {
  m_sound_manager.playGameBegins();
  m_gameView.init(fen);
  m_gameManager->startGame(playerColor, fen, vsBot);
}

void GameController::handleEvent(const sf::Event& event) {
  if (m_chess_game.getResult() == core::GameResult::ONGOING) m_inputManager.processEvent(event);
}

void GameController::render() {
  m_gameView.render();
}

void GameController::update(float dt) {
  if (m_chess_game.getResult() == core::GameResult::ONGOING) {
    m_gameView.update(dt);
    if (m_gameManager) m_gameManager->update(dt);  
  }
}

void GameController::highlightLastMove() {
  if (m_lastMoveSquares.first != core::NO_SQUARE)
    m_gameView.highlightSquare(m_lastMoveSquares.first);
  if (m_lastMoveSquares.second != core::NO_SQUARE)
    m_gameView.highlightSquare(m_lastMoveSquares.second);
}

void GameController::selectSquare(core::Square sq) {
  m_gameView.highlightSquare(sq);
  m_selected_sq = sq;
}

void GameController::deselectSquare() {
  m_gameView.clearAllHighlights();
  highlightLastMove();
  m_selected_sq = core::NO_SQUARE;
}

void GameController::hoverSquare(core::Square sq) {
  m_hover_sq = sq;
  m_gameView.highlightHoverSquare(m_hover_sq);
}
void GameController::dehoverSquare() {
  if (m_hover_sq != core::NO_SQUARE) m_gameView.clearHighlightHoverSquare(m_hover_sq);
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

  
  if (onClick)
    m_gameView.animationMovePiece(from, to, dEnPassantSquare, move.promotion);
  else
    m_gameView.animationDropPiece(from, to, dEnPassantSquare, move.promotion);

  
  if (move.castle != model::CastleSide::None) {
    core::Square rookSquare =
        m_chess_game.getRookSquareFromCastleside(move.castle, sideToTurnBeforeMove);
    core::Square newRookSquare;
    if (move.castle == model::CastleSide::KingSide)
      newRookSquare = to - 1;
    else
      newRookSquare = to + 1;
    m_gameView.animationMovePiece(rookSquare, newRookSquare);
  }

  
  m_lastMoveSquares = {from, to};
  deselectSquare();
  highlightLastMove();

  
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
  m_gameView.animationSnapAndReturn(sq, cur);
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
  return m_gameView.isSameColorPiece(a, b);
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
    if (m_gameView.hasPieceOnSquare(sq))
      m_gameView.highlightCaptureSquare(sq);
    else
      m_gameView.highlightAttackSquare(sq);
  }
}

void GameController::onClick(core::MousePos mousePos) {
  core::Square sq = m_gameView.mousePosToSquare(mousePos);

  if (m_gameView.isInPromotionSelection()) {
    core::PieceType promoType = m_gameView.getSelectedPromotion(mousePos);
    m_gameView.removePromotionSelection();
    if (m_gameManager) m_gameManager->completePendingPromotion(promoType);
    deselectSquare();
    return;
  }

  
  if (m_selected_sq == core::NO_SQUARE) {
    if (m_gameView.hasPieceOnSquare(sq) &&
        m_chess_game.getPiece(sq).color == m_chess_game.getGameState().sideToMove) {
      snapAndReturn(sq, mousePos);
      showAttacks(getAttackSquares(sq));
    }
    return;
  }

  
  if (m_selected_sq == sq) {
    snapAndReturn(sq, mousePos);
    deselectSquare();
    return;
  }

  
  if (tryMove(m_selected_sq, sq)) {
    
    if (m_gameManager) {
      bool accepted = m_gameManager->requestUserMove(m_selected_sq, sq, true);
      if (!accepted) {
        
        
        deselectSquare();
      }
    }
  } else {
    deselectSquare();
    if (m_gameView.hasPieceOnSquare(sq) && isSameColor(m_selected_sq, sq)) {
      snapAndReturn(sq, mousePos);
      showAttacks(getAttackSquares(sq));
    }
  }
}

void GameController::onDrag(core::MousePos start, core::MousePos current) {
  core::Square sqStart = m_gameView.mousePosToSquare(start);
  core::Square sqMous = m_gameView.mousePosToSquare(current);

  if (m_gameView.isInPromotionSelection()) {
    m_gameView.removePromotionSelection();
    if (m_gameManager) m_gameManager->completePendingPromotion(core::PieceType::None);
    deselectSquare();
    return;
  }

  if (!m_gameView.hasPieceOnSquare(sqStart) ||
      m_chess_game.getPiece(sqStart).color != m_chess_game.getGameState().sideToMove)
    return;

  if (m_selected_sq != sqStart) {
    deselectSquare();
    selectSquare(sqStart);
    showAttacks(getAttackSquares(sqStart));
  }

  if (m_hover_sq != sqMous) dehoverSquare();

  hoverSquare(sqMous);

  m_gameView.setPieceToMouseScreenPos(sqStart, current);
  m_gameView.playPiecePlaceHolderAnimation(sqStart);
}

void GameController::onDrop(core::MousePos start, core::MousePos end) {
  core::Square from = m_gameView.mousePosToSquare(start);
  core::Square to = m_gameView.mousePosToSquare(end);

  dehoverSquare();

  if (m_gameView.isInPromotionSelection()) {
    m_gameView.removePromotionSelection();
    if (m_gameManager) m_gameManager->completePendingPromotion(core::PieceType::None);
    deselectSquare();
    return;
  }

  if (!m_gameView.hasPieceOnSquare(from) ||
      m_chess_game.getPiece(from).color != m_chess_game.getGameState().sideToMove) {
    deselectSquare();
    return;
  }
  m_gameView.endAnimation(from);

  if (from != to && tryMove(from, to)) {
    if (m_gameManager) {
      bool accepted = m_gameManager->requestUserMove(from, to, false);
      if (!accepted) {
        deselectSquare();
      }
    }
  } else {
    if (m_chess_game.isKingInCheck(m_chess_game.getGameState().sideToMove) && from != to) {
      m_gameView.warningKingSquareAnim(
          m_chess_game.getKingSquare(m_chess_game.getGameState().sideToMove));
      m_sound_manager.playWarning();
    }
    m_gameView.setPieceToSquareScreenPos(from, from);
    selectSquare(from);
  }
}

}  
