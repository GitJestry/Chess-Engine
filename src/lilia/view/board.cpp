#include "lilia/view/board.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <string>

#include "lilia/view/texture_table.hpp"

namespace lilia::view {

Board::Board(Entity::Position pos) : Entity(pos) {}

void Board::init(const sf::Texture &textureWhite, const sf::Texture &textureBlack,
                 const sf::Texture &textureBoard) {
  setTexture(textureBoard);
  setScale(constant::WINDOW_PX_SIZE, constant::WINDOW_PX_SIZE);

  sf::Vector2f board_offset(
      getPosition().x - constant::WINDOW_PX_SIZE / 2 + constant::SQUARE_PX_SIZE / 2,
      getPosition().y - constant::WINDOW_PX_SIZE / 2 + constant::SQUARE_PX_SIZE / 2);

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

  float offset = constant::SQUARE_PX_SIZE * 0.2f;
  float bottom = board_offset.y + constant::BOARD_SIZE * constant::SQUARE_PX_SIZE -
                 constant::SQUARE_PX_SIZE / 2 - offset;
  float left = board_offset.x - constant::SQUARE_PX_SIZE / 2 + offset;

  for (int file = 0; file < constant::BOARD_SIZE; ++file) {
    std::string name = constant::ASSET_PIECES_FILE_PATH + "/file_" +
                       std::string(1, static_cast<char>('a' + file)) + ".png";
    m_file_labels[file].setTexture(TextureTable::getInstance().get(name));
    auto size = m_file_labels[file].getOriginalSize();
    float scale = (constant::SQUARE_PX_SIZE * 0.2f) / size.x;
    m_file_labels[file].setScale(scale, scale);
    float x = board_offset.x + file * constant::SQUARE_PX_SIZE;
    m_file_labels[file].setPosition({x, bottom});
  }

  for (int rank = 0; rank < constant::BOARD_SIZE; ++rank) {
    std::string name = constant::ASSET_PIECES_FILE_PATH + "/rank_" +
                       std::to_string(rank + 1) + ".png";
    m_rank_labels[rank].setTexture(TextureTable::getInstance().get(name));
    auto size = m_rank_labels[rank].getOriginalSize();
    float scale = (constant::SQUARE_PX_SIZE * 0.2f) / size.x;
    m_rank_labels[rank].setScale(scale, scale);
    float y = board_offset.y + (constant::BOARD_SIZE - 1 - rank) *
                             constant::SQUARE_PX_SIZE;
    m_rank_labels[rank].setPosition({left, y});
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

  for (int rank = 0; rank < constant::BOARD_SIZE; ++rank) {
    for (int file = 0; file < constant::BOARD_SIZE; ++file) {
      int index = file + rank * constant::BOARD_SIZE;

      float x = board_offset.x + file * constant::SQUARE_PX_SIZE;
      float y = board_offset.y + (constant::BOARD_SIZE - 1 - rank) * constant::SQUARE_PX_SIZE;

      m_squares[index].setPosition({x, y});
    }
  }

  float offset = constant::SQUARE_PX_SIZE * 0.2f;
  float bottom = board_offset.y + constant::BOARD_SIZE * constant::SQUARE_PX_SIZE -
                 constant::SQUARE_PX_SIZE / 2 - offset;
  float left = board_offset.x - constant::SQUARE_PX_SIZE / 2 + offset;

  for (int file = 0; file < constant::BOARD_SIZE; ++file) {
    float x = board_offset.x + file * constant::SQUARE_PX_SIZE;
    m_file_labels[file].setPosition({x, bottom});
  }

  for (int rank = 0; rank < constant::BOARD_SIZE; ++rank) {
    float y = board_offset.y + (constant::BOARD_SIZE - 1 - rank) *
                             constant::SQUARE_PX_SIZE;
    m_rank_labels[rank].setPosition({left, y});
  }
}

}  // namespace lilia::view
