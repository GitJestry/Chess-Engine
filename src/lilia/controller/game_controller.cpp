#include "lilia/controller/game_controller.hpp"

#include <SFML/System/Sleep.hpp>
#include <SFML/System/Time.hpp>
#include <iostream>

namespace lilia {

GameController::GameController(GameView& gView, ChessGame& game)
    : m_gameView(gView), m_chess_game(game) {
  m_inputManager.setOnClick([this](core::MousePos pos) { this->onClick(pos); });

  m_inputManager.setOnDrag(
      [this](core::MousePos start, core::MousePos current) { this->onDrag(start, current); });

  m_inputManager.setOnDrop(
      [this](core::MousePos start, core::MousePos end) { this->onDrop(start, end); });
  gView.init();
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
  if (m_lastMoveSquares.first != core::Square::NONE)
    m_gameView.highlightSquare(m_lastMoveSquares.first);
  if (m_lastMoveSquares.second != core::Square::NONE)
    m_gameView.highlightSquare(m_lastMoveSquares.second);
}

void GameController::selectSquare(core::Square sq) {
  m_gameView.highlightSquare(sq);
  m_selected_sq = sq;
}

void GameController::deselectSquare() {
  m_gameView.clearAllHighlights();
  m_selected_sq = core::Square::NONE;
}

void GameController::hoverSquare(core::Square sq) {
  m_hover_sq = sq;
  m_gameView.highlightHoverSquare(m_hover_sq);
}
void GameController::dehoverSquare() {
  if (m_hover_sq != core::Square::NONE) m_gameView.clearHighlightHoverSquare(m_hover_sq);
  m_hover_sq = core::Square::NONE;
}

void GameController::movePieceAndClear(core::Square from, core::Square to, bool onClick) {
  if (onClick)
    m_gameView.animationMovePiece(from, to);
  else
    m_gameView.animationDropPiece(from, to);

  m_lastMoveSquares = {from, to};
  deselectSquare();
  highlightLastMove();
}

void GameController::snapAndReturn(core::Square sq, core::MousePos cur) {
  selectSquare(sq);
  m_gameView.animationSnapAndReturn(sq, cur);
}

// TODO: placeholder
[[nodiscard]] bool GameController::tryMove(core::Square a, core::Square b) {
  for (auto att : getAttackSquares(a)) {
    if (att == b) return true;
  }

  return false;
}
// TODO: placeholder
[[nodiscard]] bool isSameColor(core::Square a, core::Square b) {
  return true;
}

[[nodiscard]] std::vector<core::Square> GameController::getAttackSquares(
    core::Square pieceSQ) const {
  // TODO:
  return {core::Square::A4, core::Square::B4};
}

void GameController::showAttacks(std::vector<core::Square> att) {
  for (auto sq : att) m_gameView.highlightAttackSquare(sq);
}

void GameController::onClick(core::MousePos mousePos) {
  core::Square sq = m_gameView.mousePosToSquare(mousePos);

  // Keine Auswahl
  if (m_selected_sq == core::Square::NONE) {
    if (m_gameView.hasPieceOnSquare(sq)) {
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
    movePieceAndClear(m_selected_sq, sq, true);
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

  if (!m_gameView.hasPieceOnSquare(sqStart)) return;

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

  if (!m_gameView.hasPieceOnSquare(from)) {
    deselectSquare();
    return;
  }

  m_gameView.endAnimation(from);

  if (tryMove(from, to)) {
    movePieceAndClear(from, to, false);
  } else {
    m_gameView.setPieceToSquareScreenPos(from, from);
    selectSquare(from);
  }
}

}  // namespace lilia
