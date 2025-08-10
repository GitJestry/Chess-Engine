#include "visuals/game_view.hpp"

GameView::GameView(sf::RenderWindow& window)
    : m_board({WINDOW_SIZE / 2, WINDOW_SIZE / 2}), m_pieces(), m_window_ref(window) {}

void GameView::init(const std::string& fen) {
  TextureTable::getInstance().preloadTextures();

  m_board.init(TextureTable::getInstance().get("white"), TextureTable::getInstance().get("black"),
               TextureTable::getInstance().get("transparent"));

  std::string boardPart = fen.substr(0, fen.find(' '));
  int rank = 7;
  int file = 0;
  for (char ch : boardPart) {
    if (ch == '/') {
      // Nächste Reihe
      rank--;
      file = 0;
    } else if (std::isdigit(ch)) {
      // So viele leere Felder überspringen
      file += ch - '0';
    } else {
      int pos = file + rank * BOARD_SIZE;
      PieceType type;
      switch (std::tolower(ch)) {
        case 'k':
          type = KING;
          break;
        case 'p':
          type = PAWN;
          break;
        case 'n':
          type = KNIGHT;
          break;
        case 'b':
          type = BISHOP;
          break;
        case 'r':
          type = ROOK;
          break;
        default:
          type = QUEEN;
          break;
      }

      addPiece(type, (std::isupper(ch) == 1 ? WHITE : BLACK), static_cast<Square>(pos));
      file++;
    }
  }
}

void GameView::renderBoard() {
  m_board.draw(m_window_ref);
  for (auto& pair : m_pieces) {
    const auto& pos = pair.first;
    auto& piece = pair.second;
    piece.setPosition(m_board.getSquares()[pos].getPosition());
    piece.draw(m_window_ref);
  }
}

void GameView::resetBoard() {
  m_pieces.clear();
}

void GameView::addPiece(PieceType type, PieceColor color, Square pos) {
  int numTypes = 6;
  std::string filename =
      "C:/Users/julia/OneDrive/Desktop/vs projects/Chess-Engine/PNG_Designs/piece_" +
      std::to_string(type + numTypes * color) + ".png";

  const sf::Texture& texture = TextureTable::getInstance().get(filename);
  // Figur erzeugen
  Piece newpiece(color, type, texture);
  newpiece.setScale(1.5f, 1.5f);
  m_pieces[pos] = std::move(newpiece);
}

void GameView::removePiece(Square pos) {}

void GameView::movePiece(Square from, Square to) {}

void GameView::highlightSquare(Square pos) {}

void GameView::clearHighlights() {}

void GameView::updateTurnIndicator(PieceColor activeColor) {}

void GameView::showMessage(const std::string& message) {}
