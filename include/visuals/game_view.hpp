#pragma once

#include <SFML/Graphics.hpp>
#include <bitboard.hpp>
#include <unordered_map>

#include "./board.hpp"
#include "./piece.hpp"
#include "./texture_table.hpp"

class GameView {
 public:
  ~GameView() = default;
  GameView(sf::RenderWindow& window);

  // Initialisiert die Ansicht, lädt Ressourcen
  void init(const std::string& fen = START_FEN);

  // Zeichnet das gesamte Brett neu
  void renderBoard();

  // Setzt ein neues Spiel (alle Figuren zurücksetzen)
  void resetBoard();

  // Fügt eine Figur an einer Position hinzu
  void addPiece(PieceType type, PieceColor color, Square pos);

  // Entfernt eine Figur von einer Position
  void removePiece(Square pos);

  // Bewegt eine Figur von Start- zu Zielposition (rein visuell)
  void movePiece(Square from, Square to);

  // Hebt ein Feld visuell hervor (z. B. möglichen Zug)
  void highlightSquare(Square pos);

  // Entfernt alle Markierungen
  void clearHighlights();

  // Zeigt wessen Zug ist
  void updateTurnIndicator(PieceColor activeColor);

  // Zeigt eine Meldung im UI (z. B. Schach, Matt, Remis)
  void showMessage(const std::string& message);

 private:
  Board m_board;
  std::unordered_map<Square, Piece> m_pieces;
  sf::RenderWindow& m_window_ref;
};
