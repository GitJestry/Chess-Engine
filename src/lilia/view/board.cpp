#include "lilia/view/board.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <string>

#include "lilia/view/texture_table.hpp"

namespace lilia::view {

Board::Board(Entity::Position pos) : Entity(pos) {}

void Board::init(const sf::Texture &textureWhite, const sf::Texture &textureBlack,
                 const sf::Texture &textureBoard) {
  setTexture(textureBoard);
  setOriginToCenter();
  setScale(constant::WINDOW_PX_SIZE, constant::WINDOW_PX_SIZE);

  sf::Vector2f board_offset(
      getPosition().x - constant::WINDOW_PX_SIZE / 2 + constant::SQUARE_PX_SIZE / 2,
      getPosition().y - constant::WINDOW_PX_SIZE / 2 + constant::SQUARE_PX_SIZE / 2);

  // Squares aufbauen
  for (int rank = 0; rank < constant::BOARD_SIZE; ++rank) {
    for (int file = 0; file < constant::BOARD_SIZE; ++file) {
      int index = file + rank * constant::BOARD_SIZE;

      float x = board_offset.x + file * constant::SQUARE_PX_SIZE;
      float y = board_offset.y + (constant::BOARD_SIZE - 1 - rank) * constant::SQUARE_PX_SIZE;

      m_squares[index].setPosition({x, y});
      if ((rank + file) % 2 == 0)
        m_squares[index].setTexture(textureBlack);
      else
        m_squares[index].setTexture(textureWhite);
      m_squares[index].setScale(constant::SQUARE_PX_SIZE, constant::SQUARE_PX_SIZE);
      m_squares[index].setOriginToCenter();
    }
  }

  // kleines Padding, damit Labels nicht am Rand kleben
  const float pad = constant::SQUARE_PX_SIZE * 0.04f;

  // FILE-Labels (a–h): unten rechts IM Feld der Grundreihe (rank 0 visuell unten)
  for (int file = 0; file < constant::BOARD_SIZE; ++file) {
    std::string name = constant::ASSET_PIECES_FILE_PATH + "/" +
                       std::string(1, static_cast<char>('a' + file)) + ".png";
    m_file_labels[file].setTexture(TextureTable::getInstance().get(name));

    auto size = m_file_labels[file].getOriginalSize();
    float scale = (constant::SQUARE_PX_SIZE * 0.25f) / size.x;
    m_file_labels[file].setScale(scale, scale);

    // Zentrum des Ziel-Feldes (rank 0 ist unten)
    float cx = board_offset.x + file * constant::SQUARE_PX_SIZE;
    float cy = board_offset.y + (constant::BOARD_SIZE - 1 - 0) * constant::SQUARE_PX_SIZE;

    // skalierte Größe (für Offsets ohne Origin-Änderung)
    float w = size.x * scale;
    float h = size.y * scale;

    // unten rechts im Feld (Top-Left-Koordinate des Sprites an die Ecke minus w/h)
    m_file_labels[file].setPosition({cx + constant::SQUARE_PX_SIZE * 0.5f - pad - w,
                                     cy + constant::SQUARE_PX_SIZE * 0.5f - pad - h});
  }

  // RANK-Labels (1–8): oben links IM Feld der linken Spalte (file 0)
  for (int rank = 0; rank < constant::BOARD_SIZE; ++rank) {
    std::string name = constant::ASSET_PIECES_FILE_PATH + "/" + std::to_string(rank + 1) + ".png";
    m_rank_labels[rank].setTexture(TextureTable::getInstance().get(name));

    auto size = m_rank_labels[rank].getOriginalSize();
    float scale = (constant::SQUARE_PX_SIZE * 0.25f) / size.x;
    m_rank_labels[rank].setScale(scale, scale);

    float cx = board_offset.x;  // linkes Feld (file 0)
    float cy = board_offset.y + (constant::BOARD_SIZE - 1 - rank) * constant::SQUARE_PX_SIZE;

    // oben links im Feld (Origin ist (0,0) → direkt auf die Ecke setzen)
    m_rank_labels[rank].setPosition(
        {cx - constant::SQUARE_PX_SIZE * 0.5f + pad, cy - constant::SQUARE_PX_SIZE * 0.5f + pad});
  }
}

[[nodiscard]] Entity::Position Board::getPosOfSquare(core::Square sq) const {
  return m_squares[static_cast<size_t>(sq)].getPosition();
}

void Board::draw(sf::RenderWindow &window) {
  Entity::draw(window);

  for (auto &s : m_squares) {
    s.draw(window);
  }
  for (auto &l : m_file_labels) {
    l.draw(window);
  }
  for (auto &l : m_rank_labels) {
    l.draw(window);
  }
}

void Board::setPosition(const Entity::Position &pos) {
  Entity::setPosition(pos);
  Entity::Position board_offset(
      getPosition().x - constant::WINDOW_PX_SIZE / 2 + constant::SQUARE_PX_SIZE / 2,
      getPosition().y - constant::WINDOW_PX_SIZE / 2 + constant::SQUARE_PX_SIZE / 2);

  // Squares neu positionieren
  for (int rank = 0; rank < constant::BOARD_SIZE; ++rank) {
    for (int file = 0; file < constant::BOARD_SIZE; ++file) {
      int index = file + rank * constant::BOARD_SIZE;

      float x = board_offset.x + file * constant::SQUARE_PX_SIZE;
      float y = board_offset.y + (constant::BOARD_SIZE - 1 - rank) * constant::SQUARE_PX_SIZE;

      m_squares[index].setPosition({x, y});
    }
  }

  const float pad = constant::SQUARE_PX_SIZE * 0.08f;

  // FILE-Labels wieder unten rechts im Feld der Grundreihe
  for (int file = 0; file < constant::BOARD_SIZE; ++file) {
    auto size = m_file_labels[file].getOriginalSize();
    float scale = (constant::SQUARE_PX_SIZE * 0.3f) / size.x;
    float w = size.x * scale;
    float h = size.y * scale;

    float cx = board_offset.x + file * constant::SQUARE_PX_SIZE;
    float cy = board_offset.y + (constant::BOARD_SIZE - 1 - 0) * constant::SQUARE_PX_SIZE;

    m_file_labels[file].setPosition({cx + constant::SQUARE_PX_SIZE * 0.5f - pad - w,
                                     cy + constant::SQUARE_PX_SIZE * 0.5f - pad - h});
  }

  // RANK-Labels wieder oben links im linken Randfeld
  for (int rank = 0; rank < constant::BOARD_SIZE; ++rank) {
    auto size = m_rank_labels[rank].getOriginalSize();
    float scale = (constant::SQUARE_PX_SIZE * 0.3f) / size.x;
    (void)size;
    (void)scale;  // nur falls ungenutzt-Warnungen auftreten

    float cx = board_offset.x;  // file 0
    float cy = board_offset.y + (constant::BOARD_SIZE - 1 - rank) * constant::SQUARE_PX_SIZE;

    m_rank_labels[rank].setPosition(
        {cx - constant::SQUARE_PX_SIZE * 0.5f + pad, cy - constant::SQUARE_PX_SIZE * 0.5f + pad});
  }
}

}  // namespace lilia::view
