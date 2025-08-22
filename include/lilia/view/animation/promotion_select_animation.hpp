#pragma once

#include "../../chess_types.hpp"
#include "../promotion_manager.hpp"
#include "i_animation.hpp"

namespace lilia::view::animation {

class PromotionSelectAnim : public IAnimation {
 public:
  PromotionSelectAnim(Entity::Position prPos, PromotionManager& prOptRef, core::Color c);
  void update(float dt) override;
  void draw(sf::RenderWindow& window) override;
  [[nodiscard]] inline bool isFinished() const override;

 private:
  Entity::Position m_promo_pos;
  PromotionManager& m_promo_mgr_ref;
  Entity m_white_boarder;
  Entity m_white_boarder_shadow;
};

}  // namespace lilia::view::animation
