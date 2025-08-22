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
  gView.init();
  game.setPosition(view::constant::START_FEN);
  m_sound_manager.loadSounds();
}

void GameController::handleEvent(const sf::Event& event) {
  m_inputManager.processEvent(event);
}

void GameController::render() {
  m_gameView.render();
}

void GameController::update(float dt) {
  m_gameView.update(dt);
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

void GameController::movePieceAndClear(core::Square from, core::Square to, bool onClick,
                                       core::PieceType promotion) {
  const model::Move& move = m_chess_game.getMove(from, to);
  bool isPlayerMove = (m_chess_game.getGameState().sideToMove == m_player_color);
  core::Square dEnPassantSquare = core::NO_SQUARE;

  // enpassant
  if (move.isEnPassant) {
    if (m_chess_game.getGameState().sideToMove == core::Color::White)
      dEnPassantSquare = to - 8;
    else
      dEnPassantSquare = to + 8;
  }

  // move animation
  if (onClick)
    m_gameView.animationMovePiece(from, to, dEnPassantSquare, promotion);
  else
    m_gameView.animationDropPiece(from, to, dEnPassantSquare, promotion);

  // castling
  if (move.castle != model::CastleSide::None) {
    core::Square rookSquare = m_chess_game.getRookSquareFromCastleside(move.castle);
    core::Square newRookSquare;
    if (move.castle == model::CastleSide::KingSide)
      newRookSquare = to - 1;
    else
      newRookSquare = to + 1;
    m_gameView.animationMovePiece(rookSquare, newRookSquare);
  }

  // visual highlight
  m_lastMoveSquares = {from, to};
  deselectSquare();
  highlightLastMove();
  // intern game
  m_chess_game.doMove(from, to, promotion);

  // Sound check
  if (m_chess_game.isKingInCheck(m_chess_game.getGameState().sideToMove)) {
    m_sound_manager.playCheck();
  } else {
    if (move.isCapture) {
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
  const std::vector<model::Move>& moves = m_chess_game.generateLegalMoves();
  for (auto m : moves) {
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
  const std::vector<model::Move>& moves = m_chess_game.generateLegalMoves();
  for (auto m : moves) {
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
    if (promoType != core::PieceType::None) {
      movePieceAndClear(m_selected_sq, m_promotion_square, true, promoType);
    } else {
      m_promotion_square = core::NO_SQUARE;
      deselectSquare();
    }
    return;
  }

  // Keine Auswahl
  if (m_selected_sq == core::NO_SQUARE) {
    if (m_gameView.hasPieceOnSquare(sq) &&
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

  // Versuch eines Zugs
  if (tryMove(m_selected_sq, sq)) {
    if (!isPromotion(m_selected_sq, sq)) {
      movePieceAndClear(m_selected_sq, sq, true);
    } else {
      m_promotion_square = sq;
      snapAndReturn(m_selected_sq, mousePos);
      m_gameView.playPromotionSelectAnim(sq, m_chess_game.getGameState().sideToMove);
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
    m_promotion_square = core::NO_SQUARE;
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
    m_promotion_square = core::NO_SQUARE;
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
    if (!isPromotion(from, to)) {
      movePieceAndClear(from, to, false);
    } else {
      m_promotion_square = to;
      snapAndReturn(from, end);
      m_gameView.playPromotionSelectAnim(to, m_chess_game.getGameState().sideToMove);
    }
  } else {
    m_gameView.setPieceToSquareScreenPos(from, from);
    selectSquare(from);
  }
}

}  // namespace lilia::controller
