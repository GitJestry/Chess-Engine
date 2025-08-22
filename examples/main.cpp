#include <iostream>
#include <string>

#include "lilia/controller/game_controller.hpp"
#include "lilia/model/chess_game.hpp"
#include "lilia/model/core/magic.hpp"
#include "lilia/view/game_view.hpp"
#include "lilia/view/texture_table.hpp"

// Annahme: Piece hat PieceType type; Color color;
char pieceToChar(const lilia::model::bb::Piece& piece) {
  if (piece.isNone()) return '.';

  char c;
  switch (piece.type) {
    case lilia::core::PieceType::King:
      c = 'k';
      break;
    case lilia::core::PieceType::Queen:
      c = 'q';
      break;
    case lilia::core::PieceType::Rook:
      c = 'r';
      break;
    case lilia::core::PieceType::Bishop:
      c = 'b';
      break;
    case lilia::core::PieceType::Knight:
      c = 'n';
      break;
    case lilia::core::PieceType::Pawn:
      c = 'p';
      break;
    default:
      c = '?';
  }

  // Großbuchstaben für Weiß
  if (piece.color == lilia::core::Color::White) c = std::toupper(c);
  return c;
}

std::string boardToString(lilia::model::ChessGame& game) {
  std::string output;
  for (int rank = 7; rank >= 0; --rank) {
    output += std::to_string(rank + 1);
    output += " ";
    for (int file = 0; file < 8; ++file) {
      int square = rank * 8 + file;
      lilia::model::bb::Piece piece = game.getPiece(square);  // deine Funktion zum Abfragen
      output += pieceToChar(piece);
      output += ' ';
    }
    output += '\n';
  }
  output += "  a b c d e f g h\n";
  return output;
}

std::string gameStateToString(const lilia::model::GameState& state) {
  std::string str;

  // Side to move
  str += (state.sideToMove == lilia::core::Color::White ? "w" : "b");
  str += " ";

  // Castling rights
  if (state.castlingRights == 0) {
    str += "-";
  } else {
    if (state.castlingRights & lilia::model::bb::Castling::WK) str += "K";
    if (state.castlingRights & lilia::model::bb::Castling::WQ) str += "Q";
    if (state.castlingRights & lilia::model::bb::Castling::BK) str += "k";
    if (state.castlingRights & lilia::model::bb::Castling::BQ) str += "q";
  }
  str += " ";

  // En passant square
  if (state.enPassantSquare == lilia::core::NO_SQUARE) {
    str += "-";
  } else {
    // Umrechnung 0..63 zu "a1".."h8"
    char file = 'a' + (state.enPassantSquare % 8);
    char rank = '1' + (state.enPassantSquare / 8);
    str += file;
    str += rank;
  }
  str += " ";

  // Halfmove clock
  str += std::to_string(state.halfmoveClock);
  str += " ";

  // Fullmove number
  str += std::to_string(state.fullmoveNumber);

  return str;
}

int main() {
  lilia::model::Zobrist::init();
  lilia::model::magic::init_magics();  // baut alle Masks + Magics durch Suche auf

  lilia::view::TextureTable::getInstance().preLoad();
  sf::RenderWindow window(
      sf::VideoMode(lilia::view::constant::WINDOW_PX_SIZE, lilia::view::constant::WINDOW_PX_SIZE),
      "Lilia", sf::Style::Titlebar | sf::Style::Close);

  lilia::model::ChessGame chessgame;
  lilia::view::GameView view(window);
  lilia::controller::GameController gController(view, chessgame);

  sf::Clock clock;

  while (window.isOpen()) {
    float dt = clock.restart().asSeconds();
    sf::Event event;
    while (window.pollEvent(event)) {
      if (event.type == sf::Event::Closed) window.close();
      gController.handleEvent(event);
    }
    gController.update(dt);
    window.clear(sf::Color::Blue);
    gController.render();
    window.display();
  }

  return 0;
}
