#pragma once

#include <array>
#include <optional>

#include "../../chess_types.hpp"
#include "../entity.hpp"
#include "i_animation.hpp"

namespace lilia::view::animation {

class PromotionSelectAnim : public IAnimation {
 public:
  PromotionSelectAnim(Entity::Position prPos, core::PieceType& prTypeRef, core::Color c);
  void update(float dt) override;
  void draw(sf::RenderWindow& window) override;
  [[nodiscard]] inline bool isFinished() const override;

 private:
  class Promotion : public Entity {
   public:
    Promotion(Entity::Position pos, core::PieceType type, core::Color color);
    Promotion() = default;
  };

  Entity::Position m_promo_pos;
  core::PieceType& m_promo_type_ref;
  std::array<Promotion, 4> m_promotions;
  Entity m_white_boarder;
};

}  // namespace lilia::view::animation
