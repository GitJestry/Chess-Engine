#include "lilia/view/piece_manager.hpp"

#include <iostream>
#include <string>

#include "lilia/view/animation/chess_animator.hpp"
#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view {

PieceManager::PieceManager(const BoardView& boardRef) : m_board_view_ref(boardRef), m_pieces() {}

void PieceManager::initFromFen(const std::string& fen) {
  std::string boardPart = fen.substr(0, fen.find(' '));
  int rank = 7;
  int file = 0;
  for (char ch : boardPart) {
    if (ch == '/') {
      rank--;
      file = 0;
    } else if (std::isdigit(ch)) {
      file += ch - '0';
    } else {
      int pos = file + rank * constant::BOARD_SIZE;
      core::PieceType type;
      switch (std::tolower(ch)) {
        case 'k':
          type = core::PieceType::King;
          break;
        case 'p':
          type = core::PieceType::Pawn;
          break;
        case 'n':
          type = core::PieceType::Knight;
          break;
        case 'b':
          type = core::PieceType::Bishop;
          break;
        case 'r':
          type = core::PieceType::Rook;
          break;
        default:
          type = core::PieceType::Queen;
          break;
      }

      addPiece(type, (std::isupper(ch) == 1 ? core::Color::White : core::Color::Black),
               static_cast<core::Square>(pos));

      file++;
    }
  }
}

[[nodiscard]] Entity::ID_type PieceManager::getPieceID(core::Square pos) const {
  if (pos == core::NO_SQUARE) return 0;
  auto ghost = m_premove_pieces.find(pos);
  if (ghost != m_premove_pieces.end()) return ghost->second.getId();
  if (m_hidden_squares.count(pos) > 0) return 0;
  auto it = m_pieces.find(pos);
  return it != m_pieces.end() ? it->second.getId() : 0;
}

[[nodiscard]] bool PieceManager::isSameColor(core::Square sq1, core::Square sq2) const {
  auto getPiece = [this](core::Square sq) -> const Piece * {
    auto ghost = m_premove_pieces.find(sq);
    if (ghost != m_premove_pieces.end()) return &ghost->second;
    if (m_hidden_squares.count(sq) > 0) return nullptr;
    auto it = m_pieces.find(sq);
    return it != m_pieces.end() ? &it->second : nullptr;
  };
  const Piece *p1 = getPiece(sq1);
  const Piece *p2 = getPiece(sq2);
  if (!p1 || !p2) return false;
  return p1->getColor() == p2->getColor();
}

Entity::Position PieceManager::createPiecePositon(core::Square pos) {
  return m_board_view_ref.getSquareScreenPos(pos) +
         Entity::Position{0.f, constant::SQUARE_PX_SIZE * 0.02f};
}

void PieceManager::addPiece(core::PieceType type, core::Color color, core::Square pos) {
  std::uint8_t numTypes = 6;
  std::string filename = constant::ASSET_PIECES_FILE_PATH + "/piece_" +
                         std::to_string(static_cast<std::uint8_t>(type) +
                                        numTypes * static_cast<std::uint8_t>(color)) +
                         ".png";

  const sf::Texture& texture = TextureTable::getInstance().get(filename);

  Piece newpiece(color, type, texture);
  newpiece.setScale(constant::ASSET_PIECE_SCALE, constant::ASSET_PIECE_SCALE);
  m_pieces[pos] = std::move(newpiece);
  m_pieces[pos].setPosition(createPiecePositon(pos));
}

void PieceManager::movePiece(core::Square from, core::Square to, core::PieceType promotion) {
  Piece movingPiece = std::move(m_pieces[from]);
  removePiece(from);
  removePiece(to);

  if (promotion != core::PieceType::None) {
    addPiece(promotion, movingPiece.getColor(), to);
  } else {
    m_pieces[to] = std::move(movingPiece);
  }
}
void PieceManager::removePiece(core::Square pos) {
  m_pieces.erase(pos);
}

void PieceManager::removeAll() {
  m_pieces.clear();
}

core::PieceType PieceManager::getPieceType(core::Square pos) const {
  auto ghost = m_premove_pieces.find(pos);
  if (ghost != m_premove_pieces.end()) return ghost->second.getType();
  if (m_hidden_squares.count(pos) > 0) return core::PieceType::None;
  auto it = m_pieces.find(pos);
  return it != m_pieces.end() ? it->second.getType() : core::PieceType::None;
}

core::Color PieceManager::getPieceColor(core::Square pos) const {
  auto ghost = m_premove_pieces.find(pos);
  if (ghost != m_premove_pieces.end()) return ghost->second.getColor();
  if (m_hidden_squares.count(pos) > 0) return core::Color::White;
  auto it = m_pieces.find(pos);
  return it != m_pieces.end() ? it->second.getColor() : core::Color::White;
}

[[nodiscard]] bool PieceManager::hasPieceOnSquare(core::Square pos) const {
  if (m_premove_pieces.find(pos) != m_premove_pieces.end()) return true;
  if (m_hidden_squares.count(pos) > 0) return false;
  return m_pieces.find(pos) != m_pieces.end();
}

Entity::Position PieceManager::getPieceSize(core::Square pos) const {
  auto ghost = m_premove_pieces.find(pos);
  if (ghost != m_premove_pieces.end()) return ghost->second.getCurrentSize();
  if (m_hidden_squares.count(pos) > 0) return {0.f, 0.f};
  auto it = m_pieces.find(pos);
  if (it == m_pieces.end()) return {0.f, 0.f};
  return it->second.getCurrentSize();
}

[[nodiscard]] inline Entity::Position mouseToEntityPos(core::MousePos mousePos) {
  return static_cast<Entity::Position>(mousePos);
}

void PieceManager::setPieceToSquareScreenPos(core::Square from, core::Square to) {
  auto ghost = m_premove_pieces.find(from);
  if (ghost != m_premove_pieces.end()) {
    ghost->second.setPosition(createPiecePositon(to));
    return;
  }
  auto it = m_pieces.find(from);
  if (it != m_pieces.end() && m_hidden_squares.count(from) == 0) {
    it->second.setPosition(createPiecePositon(to));
  }
}

void PieceManager::setPieceToScreenPos(core::Square pos, core::MousePos mousePos) {
  auto ghost = m_premove_pieces.find(pos);
  if (ghost != m_premove_pieces.end()) {
    ghost->second.setPosition(mouseToEntityPos(mousePos));
    return;
  }
  auto it = m_pieces.find(pos);
  if (it != m_pieces.end() && m_hidden_squares.count(pos) == 0) {
    it->second.setPosition(mouseToEntityPos(mousePos));
  }
}
void PieceManager::setPieceToScreenPos(core::Square pos, Entity::Position entityPos) {
  auto ghost = m_premove_pieces.find(pos);
  if (ghost != m_premove_pieces.end()) {
    ghost->second.setPosition(entityPos);
    return;
  }
  auto it = m_pieces.find(pos);
  if (it != m_pieces.end() && m_hidden_squares.count(pos) == 0) {
    it->second.setPosition(entityPos);
  }
}

void PieceManager::renderPieces(sf::RenderWindow& window,
                                const animation::ChessAnimator& chessAnimRef) {
  for (auto& pair : m_pieces) {
    const auto& pos = pair.first;
    auto& piece = pair.second;
    if (m_hidden_squares.count(pos) > 0) continue;
    if (!chessAnimRef.isAnimating(piece.getId())) {
      piece.setPosition(createPiecePositon(pos));
      piece.draw(window);
    }
  }
  // Draw premove preview pieces on top of regular pieces
  for (auto& pair : m_premove_pieces) {
    pair.second.draw(window);
  }
}

void PieceManager::renderPiece(core::Square pos, sf::RenderWindow& window) {
  if (m_hidden_squares.count(pos) > 0) return;
  auto it = m_pieces.find(pos);
  if (it != m_pieces.end()) {
    it->second.draw(window);
  }
}

void PieceManager::setPremovePiece(core::Square from, core::Square to) {
  auto it = m_pieces.find(from);
  if (it == m_pieces.end()) return;
  Piece ghost = it->second;  // copy to preserve original
  ghost.setPosition(createPiecePositon(to));
  m_premove_pieces.clear();
  m_premove_pieces[to] = std::move(ghost);
  m_hidden_squares.clear();
  m_hidden_squares.insert(from);
  if (m_pieces.find(to) != m_pieces.end()) m_hidden_squares.insert(to);
}

void PieceManager::clearPremovePieces() {
  m_premove_pieces.clear();
  m_hidden_squares.clear();
}

}  // namespace lilia::view
