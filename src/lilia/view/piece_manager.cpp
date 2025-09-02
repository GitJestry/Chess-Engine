#include "lilia/view/piece_manager.hpp"

#include <iostream>
#include <string>
#include <utility>

#include "lilia/view/animation/chess_animator.hpp"
#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view {

PieceManager::PieceManager(const BoardView &boardRef) : m_board_view_ref(boardRef), m_pieces() {}

/* -------------------- FEN -------------------- */
void PieceManager::initFromFen(const std::string &fen) {
  std::string boardPart = fen.substr(0, fen.find(' '));
  int rank = 7;
  int file = 0;
  for (char ch : boardPart) {
    if (ch == '/') {
      rank--;
      file = 0;
    } else if (std::isdigit(static_cast<unsigned char>(ch))) {
      file += ch - '0';
    } else {
      int pos = file + rank * constant::BOARD_SIZE;
      core::PieceType type;
      switch (std::tolower(static_cast<unsigned char>(ch))) {
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

      addPiece(
          type,
          (std::isupper(static_cast<unsigned char>(ch)) ? core::Color::White : core::Color::Black),
          static_cast<core::Square>(pos));
      file++;
    }
  }
}

/* -------------------- Query helpers -------------------- */
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

/* -------------------- Placement -------------------- */
Entity::Position PieceManager::createPiecePositon(core::Square pos) {
  return m_board_view_ref.getSquareScreenPos(pos) +
         Entity::Position{0.f, constant::SQUARE_PX_SIZE * 0.02f};
}

void PieceManager::addPiece(core::PieceType type, core::Color color, core::Square pos) {
  std::uint8_t numTypes = 6;
  std::string filename = constant::ASSET_PIECES_FILE_PATH + std::string("/piece_") +
                         std::to_string(static_cast<std::uint8_t>(type) +
                                        numTypes * static_cast<std::uint8_t>(color)) +
                         ".png";

  const sf::Texture &texture = TextureTable::getInstance().get(filename);

  Piece newpiece(color, type, texture);
  newpiece.setScale(constant::ASSET_PIECE_SCALE, constant::ASSET_PIECE_SCALE);
  m_pieces[pos] = std::move(newpiece);
  m_pieces[pos].setPosition(createPiecePositon(pos));
}

void PieceManager::movePiece(core::Square from, core::Square to, core::PieceType promotion) {
  Piece movingPiece;

  // The piece might have been stashed in m_captured_backup if a premove
  // ghost temporarily "captured" it. Restore it if necessary so the real
  // move can proceed.
  auto fromIt = m_pieces.find(from);
  if (fromIt != m_pieces.end()) {
    movingPiece = std::move(fromIt->second);
    m_pieces.erase(fromIt);
  } else {
    auto backupIt = m_captured_backup.find(from);
    if (backupIt != m_captured_backup.end()) {
      movingPiece = std::move(backupIt->second);
      m_captured_backup.erase(backupIt);
      m_hidden_squares.erase(from);
    } else {
      // No piece to move – likely an out-of-sync premove scenario.
      return;
    }
  }

  // If the destination square holds a stashed piece, drop it.
  removePiece(to);

  if (promotion != core::PieceType::None) {
    addPiece(promotion, movingPiece.getColor(), to);
  } else {
    m_pieces[to] = std::move(movingPiece);
  }

  // The piece now occupies 'to', so ensure it's not hidden.
  m_hidden_squares.erase(to);
}

void PieceManager::removePiece(core::Square pos) {
  m_pieces.erase(pos);
  m_captured_backup.erase(pos);
  m_hidden_squares.erase(pos);
}

void PieceManager::removeAll() {
  m_pieces.clear();
}

/* -------------------- Piece info -------------------- */
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

/* -------------------- Movement helpers -------------------- */
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

/* -------------------- Rendering -------------------- */
void PieceManager::renderPieces(sf::RenderWindow &window,
                                const animation::ChessAnimator &chessAnimRef) {
  // REAL pieces only here (no ghosts) so the animator can overdraw cleanly.
  for (auto &pair : m_pieces) {
    const auto pos = pair.first;
    auto &piece = pair.second;
    if (m_hidden_squares.count(pos) > 0) continue;
    if (!chessAnimRef.isAnimating(piece.getId())) {
      piece.setPosition(createPiecePositon(pos));
      piece.draw(window);
    }
  }
}

// New: draw ghosts after animator so they always win the z-order.
void PieceManager::renderPremoveGhosts(sf::RenderWindow &window,
                                       const animation::ChessAnimator &chessAnimRef) {
  for (auto &pair : m_premove_pieces) {
    if (!chessAnimRef.isAnimating(pair.second.getId())) {
      pair.second.setPosition(createPiecePositon(pair.first));
    }
    pair.second.draw(window);
  }
}

void PieceManager::renderPiece(core::Square pos, sf::RenderWindow &window) {
  if (m_hidden_squares.count(pos) > 0) return;
  auto it = m_pieces.find(pos);
  if (it != m_pieces.end()) {
    it->second.draw(window);
  }
}

/* -------------------- Premove (ghost pieces) -------------------- */
void PieceManager::setPremovePiece(core::Square from, core::Square to,
                                   core::PieceType promotion) {
  // If the piece was already a ghost (chained premove), move that ghost
  Piece ghost;
  auto existing = m_premove_pieces.find(from);
  if (existing != m_premove_pieces.end()) {
    ghost = std::move(existing->second);
    m_premove_pieces.erase(existing);
    // Unhide the square the ghost is leaving so it can be reused
    m_hidden_squares.erase(from);
  } else {
    auto it = m_pieces.find(from);
    if (it == m_pieces.end()) return;
    ghost = it->second;  // copy to preserve original
    m_hidden_squares.insert(from);
  }

  // Override piece type/texture for promotion preview
  if (promotion != core::PieceType::None && ghost.getType() != promotion) {
    std::uint8_t numTypes = 6;
    std::string filename =
        constant::ASSET_PIECES_FILE_PATH + std::string("/piece_") +
        std::to_string(static_cast<std::uint8_t>(promotion) +
                       numTypes * static_cast<std::uint8_t>(ghost.getColor())) +
        ".png";
    const sf::Texture& tex = TextureTable::getInstance().get(filename);
    ghost.setTexture(tex);
    ghost.setType(promotion);
  }

  // If destination already has a premove ghost (chained capture), stash it too
  if (auto ghostCap = m_premove_pieces.find(to); ghostCap != m_premove_pieces.end()) {
    m_captured_backup[to] = std::move(ghostCap->second);
    m_premove_pieces.erase(ghostCap);
  } else {
    // Otherwise stash any real piece so cancel restores it
    auto captured = m_pieces.find(to);
    if (captured != m_pieces.end()) {
      m_captured_backup[to] = std::move(captured->second);
      m_pieces.erase(captured);
    }
  }

  ghost.setPosition(createPiecePositon(to));

  // Replace any previous ghost at 'to' to avoid duplicates
  m_premove_pieces.erase(to);
  m_premove_pieces[to] = std::move(ghost);

  // Ensure underlying content never shines through ghosts
  m_hidden_squares.insert(to);
}

void PieceManager::clearPremovePieces(bool restore) {
  if (restore) {
    for (auto &pair : m_captured_backup) {
      m_pieces[pair.first] = std::move(pair.second);
      m_pieces[pair.first].setPosition(createPiecePositon(pair.first));
    }
  }
  m_captured_backup.clear();
  m_premove_pieces.clear();
  m_hidden_squares.clear();
}

void PieceManager::consumePremoveGhost(core::Square /*from*/, core::Square to) {
  // Only drop the ghost marker. DO NOT toggle hidden flags here; the real move will do that.
  auto it = m_premove_pieces.find(to);
  if (it != m_premove_pieces.end()) m_premove_pieces.erase(it);
  // Intentionally leave m_hidden_squares and m_captured_backup untouched here.
}

void PieceManager::applyPremoveInstant(core::Square from, core::Square to,
                                       core::PieceType promotion) {
  // 1) Remove the ghost at 'to' (if present) but keep both squares hidden during the swap
  auto git = m_premove_pieces.find(to);
  if (git != m_premove_pieces.end()) m_premove_pieces.erase(git);

  // 2) Fetch the real moving piece (from live or backup) atomically
  Piece moving;
  if (auto it = m_pieces.find(from); it != m_pieces.end()) {
    moving = std::move(it->second);
    m_pieces.erase(it);
  } else if (auto bit = m_captured_backup.find(from); bit != m_captured_backup.end()) {
    moving = std::move(bit->second);
    m_captured_backup.erase(bit);
  } else {
    return;  // nothing to move
  }

  // 3) Remove any occupant at 'to' (including stashed victim)
  removePiece(to);

  // 4) Place the moving piece (or promotion) at 'to'
  if (promotion != core::PieceType::None) {
    addPiece(promotion, moving.getColor(), to);
  } else {
    m_pieces[to] = std::move(moving);
    m_pieces[to].setPosition(createPiecePositon(to));
  }

  // 5) Finally reveal board squares
  m_hidden_squares.erase(from);
  m_hidden_squares.erase(to);
  m_captured_backup.erase(to);
}

}  // namespace lilia::view
