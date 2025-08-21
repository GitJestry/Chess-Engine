#include "lilia/view/animation/promotion_select_animation.hpp"

#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view::animation {

PromotionSelectAnim::Promotion::Promotion(Entity::Position pos, core::PieceType type,
                                          core::Color color)
    : Entity(pos) {
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
PromotionSelectAnim::PromotionSelectAnim(Entity::Position prPos, core::PieceType& prTypeRef,
                                         core::Color c)
    : m_promo_pos(prPos), m_promo_type_ref(prTypeRef), m_promotions(), m_white_boarder() {
  for (uint8_t t = 1; t <= 4; t++) {
    Entity::Position pos = {prPos.x, prPos.y + (t - 1) * constant::SQUARE_PX_SIZE};
    m_promotions[t - 1] = Promotion(pos, static_cast<core::PieceType>(t), c);
  }
  m_white_boarder.setTexture(TextureTable::getInstance().get(constant::STR_TEXTURE_PROMOTION));
  m_white_boarder.setScale(constant::SQUARE_PX_SIZE, 4 * constant::SQUARE_PX_SIZE);
  m_white_boarder.setOriginToCenter();
  m_white_boarder.setPosition(m_promotions[2].getPosition() -
                              Entity::Position{0.f, constant::SQUARE_PX_SIZE * 0.5f});
}

void PromotionSelectAnim::update(float dt) {}

void PromotionSelectAnim::draw(sf::RenderWindow& window) {
  m_white_boarder.draw(window);
  for (auto& p : m_promotions) p.draw(window);
}
[[nodiscard]] bool PromotionSelectAnim::isFinished() const {
  return false;
}

}  // namespace lilia::view::animation
