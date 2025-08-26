#include "lilia/view/animation/promotion_select_animation.hpp"

#include "lilia/view/promotion.hpp"
#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view::animation {

PromotionSelectAnim::PromotionSelectAnim(Entity::Position prPos, PromotionManager& prOptRef,
                                         core::Color c)
    : m_promo_pos(prPos), m_promo_mgr_ref(prOptRef) {
  m_promo_mgr_ref.fillOptions(prPos, c);

  m_white_boarder.setTexture(TextureTable::getInstance().get(constant::STR_TEXTURE_PROMOTION));
  m_white_boarder.setOriginToCenter();
  m_white_boarder.setPosition(m_promo_mgr_ref.getCenterPosition());

  m_white_boarder_shadow.setTexture(
      TextureTable::getInstance().get(constant::STR_TEXTURE_PROMOTION_SHADOW));
  m_white_boarder_shadow.setOriginToCenter();
  m_white_boarder_shadow.setPosition(m_promo_mgr_ref.getCenterPosition() +
                                     Entity::Position{0.f, +4.f});
}

void PromotionSelectAnim::update(float dt) {}

void PromotionSelectAnim::draw(sf::RenderWindow& window) {
  m_white_boarder_shadow.draw(window);
  m_white_boarder.draw(window);
  m_promo_mgr_ref.render(window);
}
[[nodiscard]] bool PromotionSelectAnim::isFinished() const {
  return !m_promo_mgr_ref.hasOptions();
}

}  
