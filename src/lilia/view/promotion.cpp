#include "lilia/view/promotion.hpp"

#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view {

Promotion::Promotion(Entity::Position pos, core::PieceType type, core::Color color)
    : Entity(pos), m_type(type) {
  std::uint8_t numTypes = 6;
  std::string filename = constant::ASSET_PIECES_FILE_PATH + "/piece_" +
                         std::to_string(static_cast<std::uint8_t>(type) +
                                        numTypes * static_cast<std::uint8_t>(color)) +
                         ".png";

  const sf::Texture& texture = TextureTable::getInstance().get(filename);
  setTexture(texture);
  setScale(constant::ASSET_PIECE_SCALE, constant::ASSET_PIECE_SCALE);
  setOriginToCenter();
}

core::PieceType Promotion::getType() {
  return m_type;
}

}  
