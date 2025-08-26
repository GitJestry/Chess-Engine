#pragma once

#include "../chess_types.hpp"
#include "entity.hpp"

namespace lilia::view {

class Promotion : public Entity {
 public:
  Promotion(Entity::Position pos, core::PieceType type, core::Color color);
  core::PieceType getType();
  Promotion() = default;

 private:
  core::PieceType m_type;
};

}  
