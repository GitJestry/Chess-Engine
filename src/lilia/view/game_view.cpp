#include "lilia/view/game_view.hpp"

#include "lilia/view/animation/move_animation.hpp"
#include "lilia/view/animation/placeholder_animation.hpp"
#include "lilia/view/animation/snap_to_square_animation.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia {

GameView::GameView(sf::RenderWindow& window, ChessGame& game)
    : m_board({core::WINDOW_PX_SIZE / 2, core::WINDOW_PX_SIZE / 2}),
      m_chess_game(game),
      m_anim_manager(),
      m_pieces(),
      m_window_ref(window),
      m_hl_attack_squares(),
      m_hl_select_squares() {}

void GameView::init(const std::string& fen) {
  m_board.init(TextureTable::getInstance().get(core::STR_TEXTURE_WHITE),
               TextureTable::getInstance().get(core::STR_TEXTURE_BLACK),
               TextureTable::getInstance().get(core::STR_TEXTURE_TRANSPARENT));

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

template <typename T>
void GameView::renderEntitiesToBoard(std::unordered_map<core::Square, T>& map) {
  for (auto& pair : map) {
    const auto& pos = pair.first;
    auto& entity = pair.second;
    entity.setPosition(getSquareScreenPos(pos));
    entity.draw(m_window_ref);
  }
}

void GameView::updateAnimations(float dt) {
  m_anim_manager.update(dt);
}

void GameView::render() {
  m_board.draw(m_window_ref);
  renderEntitiesToBoard(m_hl_select_squares);
  renderEntitiesToBoard(m_hl_hover_squares);

  for (auto& pair : m_pieces) {
    const auto& pos = pair.first;
    auto& piece = pair.second;
    if (!m_anim_manager.isAnimating(piece.getId())) {
      piece.setPosition(getSquareScreenPos(pos));
      piece.draw(m_window_ref);
    }
  }

  renderEntitiesToBoard(m_hl_attack_squares);
  m_anim_manager.draw(m_window_ref);
}

void GameView::resetBoard() {
  m_pieces.clear();
  init();
}

Entity::Position mouseToEntityPos(core::MousePos mousePos) {
  return static_cast<Entity::Position>(mousePos);
}

Entity::Position GameView::getSquareScreenPos(core::Square pos) {
  return m_board.getPosOfSquare(pos);
}

void GameView::animationSnapAndReturn(core::Square sq, core::MousePos mousePos) {
  m_anim_manager.add(m_pieces[sq].getId(), std::make_unique<SnapToSquareAnim>(
                                               m_pieces[sq], mouseToEntityPos(mousePos),
                                               getSquareScreenPos(sq), core::ANIM_SNAP_SPEED));
}

void GameView::animationMovePiece(core::Square from, core::Square to) {
  m_anim_manager.add(
      m_pieces[from].getId(),
      std::make_unique<MoveAnim>(
          m_pieces[from], getSquareScreenPos(from), getSquareScreenPos(to), core::ANIM_MOVE_SPEED,
          [this](core::Square f, core::Square t) { this->movePiece(f, t); }, from, to));
}

void GameView::animationDropPiece(core::Square from, core::Square to) {
  movePiece(from, to);
}

void GameView::playPlaceHolderAnimation(core::Square sq) {
  m_anim_manager.add(m_pieces[sq].getId(), std::make_unique<PlaceholderAnim>(m_pieces[sq]));
}

void GameView::endAnimation(core::Square sq) {
  m_anim_manager.endAnim(m_pieces[sq].getId());
}

void GameView::addPiece(core::PieceType type, core::PieceColor color, core::Square pos) {
  int numTypes = 6;
  std::string filename =
      "C:/Users/julia/OneDrive/Desktop/vs projects/Chess-Engine/PNG_Designs/piece_" +
      std::to_string(type + numTypes * color) + ".png";

  const sf::Texture& texture = TextureTable::getInstance().get(filename);

  Piece newpiece(color, type, texture);
  newpiece.setScale(1.5f, 1.5f);
  m_pieces[pos] = std::move(newpiece);
  m_pieces[pos].setPosition(getSquareScreenPos(pos));
}

void GameView::removePiece(core::Square pos) {
  m_pieces.erase(pos);
}

bool GameView::hasPieceOnSquare(core::Square pos) const {
  return m_pieces.find(pos) != m_pieces.end();
}

void GameView::movePiece(core::Square from, core::Square to) {
  Piece movingPiece = std::move(m_pieces[from]);
  m_pieces.erase(from);
  m_pieces.erase(to);
  m_pieces[to] = std::move(movingPiece);
}

void GameView::hlightSquare(core::Square pos) {
  Entity newSelectHlight(TextureTable::getInstance().get(core::STR_TEXTURE_SELECTHLIGHT));
  newSelectHlight.setScale(core::SQUARE_PX_SIZE, core::SQUARE_PX_SIZE);
  m_hl_select_squares[pos] = std::move(newSelectHlight);
}
void GameView::hlightHoverSquare(core::Square pos) {
  Entity newHoverHlight(TextureTable::getInstance().get(core::STR_TEXTURE_HOVERHLIGHT));
  m_hl_hover_squares[pos] = std::move(newHoverHlight);
}
void GameView::hlightAttackSquare(core::Square pos) {
  Entity newAttackHlight(TextureTable::getInstance().get(core::STR_TEXTURE_ATTACKHLIGHT));
  m_hl_attack_squares[pos] = std::move(newAttackHlight);
}

void GameView::clearHlightSquare(core::Square pos) {
  m_hl_select_squares.erase(pos);
}

void GameView::clearHlightHoverSquare(core::Square pos) {
  m_hl_hover_squares.erase(pos);
}

void GameView::clearAllHlights() {
  m_hl_select_squares.clear();
  m_hl_attack_squares.clear();
}

void GameView::updateTurnIndicator(core::PieceColor activeColor) {}

void GameView::showMessage(const std::string& message) {}

core::MousePos GameView::squareToMousePos(core::Square sq) {
  // Rank/File aus Stockfish-Index holen
  int file = static_cast<int>(sq) % 8;           // 0 = A, 7 = H
  int rankFromWhite = static_cast<int>(sq) / 8;  // 0 = weiße Grundreihe, 7 = schwarze Grundreihe

  // Für SFML-Koordinaten: y=0 ist oben → invertieren
  int rankSFML = 7 - rankFromWhite;

  // Mittelpunkt des Feldes in Pixeln
  int x = file * core::SQUARE_PX_SIZE + core::SQUARE_PX_SIZE / 2;
  int y = rankSFML * core::SQUARE_PX_SIZE + core::SQUARE_PX_SIZE / 2;

  return core::MousePos(x, y);
}

core::Square GameView::mousePosToSquare(core::MousePos mousePos) {
  int file = mousePos.x / core::SQUARE_PX_SIZE;      // 0 = A, 7 = H
  int rankSFML = mousePos.y / core::SQUARE_PX_SIZE;  // 0 = top row, 7 = bottom row

  int rankFromWhite = 7 - rankSFML;

  // Clamp values to be safe
  if (file < 0) file = 0;
  if (file > 7) file = 7;
  if (rankFromWhite < 0) rankFromWhite = 0;
  if (rankFromWhite > 7) rankFromWhite = 7;

  // Index in Stockfish-like format
  return static_cast<core::Square>(rankFromWhite * 8 + file);
}

void GameView::setPieceToMouseScreenPos(core::Square pos, core::MousePos mousePos) {
  m_pieces[pos].setPosition(mouseToEntityPos(mousePos));
}
void GameView::setPieceToSquareScreenPos(core::Square from, core::Square to) {
  m_pieces[from].setPosition(getSquareScreenPos(to));
}

}  // namespace lilia
