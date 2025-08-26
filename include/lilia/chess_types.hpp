#pragma once
#include <cstdint>
namespace lilia::core {
using Square = std::uint8_t;      
constexpr Square NO_SQUARE = 64;  
enum class PieceType : std::uint8_t { Pawn, Knight, Bishop, Rook, Queen, King, None };
enum class Color : std::uint8_t { White = 0, Black = 1 };
constexpr inline core::Color operator~(core::Color c) {
  return c == core::Color::White ? core::Color::Black : core::Color::White;
}
}  
