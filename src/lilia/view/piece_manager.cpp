#include "lilia/view/piece_manager.hpp"

#include <string>

#include "lilia/view/animation/chess_animator.hpp"
#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia {

PieceManager::PieceManager(const BoardView& boardRef) : m_board_view_ref(boardRef), m_pieces() {}

void PieceManager::initFromFen(std::string& fen) {
  int rank = 7;
  int file = 0;
  for (char ch : fen) {
    if (ch == '/') {
      // Nächste Reihe
      rank--;
      file = 0;
    } else if (std::isdigit(ch)) {
      // So viele leere Felder überspringen
      file += ch - '0';
    } else {
      int pos = file + rank * core::BOARD_SIZE;
      core::PieceType type;
      switch (std::tolower(ch)) {
        case 'k':
          type = core::PieceType::KING;
          break;
        case 'p':
          type = core::PieceType::PAWN;
          break;
        case 'n':
          type = core::PieceType::KNIGHT;
          break;
        case 'b':
          type = core::PieceType::BISHOP;
          break;
        case 'r':
          type = core::PieceType::ROOK;
          break;
        default:
          type = core::PieceType::QUEEN;
          break;
      }

      addPiece(type, (std::isupper(ch) == 1 ? core::PieceColor::WHITE : core::PieceColor::BLACK),
               static_cast<core::Square>(pos));

      file++;
    }
  }
}

[[nodiscard]] Entity::ID_type PieceManager::getPieceID(core::Square pos) const {
  return m_pieces.find(pos)->second.getId();
}

[[nodiscard]] bool PieceManager::isSameColor(core::Square sq1, core::Square sq2) const {
  return (m_pieces.find(sq1)->second.getColor() == m_pieces.find(sq2)->second.getColor());
}

void PieceManager::addPiece(core::PieceType type, core::PieceColor color, core::Square pos) {
  int numTypes = 6;
  std::string filename =
      core::ASSET_PIECES_FILE_PATH + "/piece_" + std::to_string(type + numTypes * color) + ".png";

  const sf::Texture& texture = TextureTable::getInstance().get(filename);

  Piece newpiece(color, type, texture);
  newpiece.setScale(1.5f, 1.5f);
  m_pieces[pos] = std::move(newpiece);
  m_pieces[pos].setPosition(m_board_view_ref.getSquareScreenPos(pos));
}

void PieceManager::movePiece(core::Square from, core::Square to) {
  Piece movingPiece = std::move(m_pieces[from]);
  m_pieces.erase(from);
  m_pieces.erase(to);
  m_pieces[to] = std::move(movingPiece);
}
void PieceManager::removePiece(core::Square pos) {
  m_pieces.erase(pos);
}

void PieceManager::removeAll() {
  m_pieces.clear();
}

[[nodiscard]] bool PieceManager::hasPieceOnSquare(core::Square pos) const {
  return m_pieces.find(pos) != m_pieces.end();
}

[[nodiscard]] inline Entity::Position mouseToEntityPos(core::MousePos mousePos) {
  return static_cast<Entity::Position>(mousePos);
}

void PieceManager::setPieceToSquareScreenPos(core::Square from, core::Square to) {
  m_pieces[from].setPosition(m_board_view_ref.getSquareScreenPos(to));
}

void PieceManager::setPieceToScreenPos(core::Square pos, core::MousePos mousePos) {
  m_pieces[pos].setPosition(mouseToEntityPos(mousePos));
}
void PieceManager::setPieceToScreenPos(core::Square pos, Entity::Position entityPos) {
  m_pieces[pos].setPosition(entityPos);
}

void PieceManager::renderPieces(sf::RenderWindow& window, const ChessAnimator& chessAnimRef) {
  for (auto& pair : m_pieces) {
    const auto& pos = pair.first;
    auto& piece = pair.second;
    if (!chessAnimRef.isAnimating(piece.getId())) {
      piece.setPosition(m_board_view_ref.getSquareScreenPos(pos));
      piece.draw(window);
    }
  }
}

void PieceManager::renderPiece(core::Square pos, sf::RenderWindow& window) {
  m_pieces.find(pos)->second.draw(window);
}

}  // namespace lilia
