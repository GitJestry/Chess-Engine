#include "lilia/model/board.hpp"

#include <cassert>

namespace lilia::model {

namespace {
constexpr std::int8_t kTypeIndex[7] = {0, 1, 2, 3, 4, 5, -1};
inline int type_index(core::PieceType t) noexcept {
  return kTypeIndex[static_cast<int>(t)];
}

// Packed byte layout:
//  low 3 bits: (typeIndex + 1) in [1..6], 0 means empty
//  bit 3: color (0 white, 1 black)
inline int decode_ti(std::uint8_t packed) noexcept {
  return (packed & 0x7) - 1;
}  // only if packed!=0
inline int decode_ci(std::uint8_t packed) noexcept {
  return (packed >> 3) & 0x1;
}  // only if packed!=0
}  // namespace

// ---- Board ----

Board::Board() {
  clear();
}

void Board::clear() noexcept {
  for (auto& byColor : m_bb) byColor.fill(0);
  m_color_occ = {0, 0};
  m_all_occ = 0;
  m_piece_on.fill(0);
}

inline std::uint8_t Board::pack_piece(bb::Piece p) noexcept {
  if (p.type == core::PieceType::None) return 0;
  const int ti = type_index(p.type);  // 0..5
  assert(ti >= 0 && ti < 6 && "Invalid PieceType");
  const std::uint8_t c = static_cast<std::uint8_t>(bb::ci(p.color) & 1u);  // 0=white,1=black
  return static_cast<std::uint8_t>((ti + 1) | (c << 3));  // low3: (ti+1), bit3: color
}

inline bb::Piece Board::unpack_piece(std::uint8_t pp) noexcept {
  if (pp == 0) return bb::Piece{core::PieceType::None, core::Color::White};
  const int ti = (pp & 0x7) - 1;  // 0..5
  const core::PieceType pt = static_cast<core::PieceType>(ti);
  const core::Color col = ((pp >> 3) & 1u) ? core::Color::Black : core::Color::White;
  return bb::Piece{pt, col};
}

void Board::setPiece(core::Square sq, bb::Piece p) noexcept {
  const int s = static_cast<int>(sq);
  assert(s >= 0 && s < 64);

  const std::uint8_t newPacked = pack_piece(p);
  const std::uint8_t oldPacked = m_piece_on[s];

  // Fast path: same piece already there → nothing to do
  if (oldPacked == newPacked) return;

  // If something stands there, remove it (without constructing a bb::Piece)
  if (oldPacked) {
    const int oldTi = decode_ti(oldPacked);
    const int oldCi = decode_ci(oldPacked);
    const bb::Bitboard mask = bb::sq_bb(sq);

    m_bb[oldCi][oldTi] &= ~mask;
    m_color_occ[oldCi] &= ~mask;
    m_all_occ &= ~mask;

    m_piece_on[s] = 0;
  }

  // Place new (if not empty)
  if (newPacked) {
    const int ti = type_index(p.type);
    assert(ti >= 0 && ti < 6 && "Invalid PieceType in setPiece");
    const int ci = bb::ci(p.color);
    const bb::Bitboard mask = bb::sq_bb(sq);

    m_bb[ci][ti] |= mask;
    m_color_occ[ci] |= mask;
    m_all_occ |= mask;

    m_piece_on[s] = newPacked;
  }
}

void Board::removePiece(core::Square sq) noexcept {
  const int s = static_cast<int>(sq);
  assert(s >= 0 && s < 64);

  const std::uint8_t packed = m_piece_on[s];
  if (!packed) return;  // already empty

  // Decode directly from packed (avoid constructing bb::Piece)
  const int ti = decode_ti(packed);
  const int ci = decode_ci(packed);
  assert(ti >= 0 && ti < 6 && (ci == 0 || ci == 1));

  const bb::Bitboard mask = bb::sq_bb(sq);

  m_bb[ci][ti] &= ~mask;
  m_color_occ[ci] &= ~mask;
  m_all_occ &= ~mask;

  m_piece_on[s] = 0;
}

std::optional<bb::Piece> Board::getPiece(core::Square sq) const noexcept {
  const std::uint8_t packed = m_piece_on[static_cast<int>(sq)];
  if (!packed) return std::nullopt;
  return unpack_piece(packed);
}

void Board::movePiece_noCapture(core::Square from, core::Square to) noexcept {
  const int sf = static_cast<int>(from);
  const int st = static_cast<int>(to);
  assert(sf >= 0 && sf < 64 && st >= 0 && st < 64);

  const std::uint8_t packed = m_piece_on[sf];
  if (!packed) return;  // nothing to move
  assert(m_piece_on[st] == 0 && "movePiece_noCapture: 'to' must be empty");

  const int ti = decode_ti(packed);
  const int ci = decode_ci(packed);
  assert(ti >= 0 && ti < 6 && (ci == 0 || ci == 1));

  const bb::Bitboard fromMask = bb::sq_bb(from);
  const bb::Bitboard toMask = bb::sq_bb(to);

  // Piece bitboard
  m_bb[ci][ti] = (m_bb[ci][ti] & ~fromMask) | toMask;

  // Occupancies (robust form rather than XOR)
  m_color_occ[ci] = (m_color_occ[ci] & ~fromMask) | toMask;
  m_all_occ = (m_all_occ & ~fromMask) | toMask;

  // By-square
  m_piece_on[sf] = 0;
  m_piece_on[st] = packed;
}

void Board::movePiece_withCapture(core::Square from, core::Square capSq, core::Square to,
                                  bb::Piece captured) noexcept {
  const int sf = static_cast<int>(from);
  const int sc = static_cast<int>(capSq);
  const int st = static_cast<int>(to);

  const std::uint8_t moverPacked = m_piece_on[sf];
  if (!moverPacked) return;  // nothing to move

  // Decode mover
  const int m_ti = (moverPacked & 0x7) - 1;   // 0..5
  const int m_ci = (moverPacked >> 3) & 0x1;  // 0/1
  assert(m_ti >= 0 && m_ti < 6);

  // Decode captured (must exist)
  assert(captured.type != core::PieceType::None);
  const std::uint8_t capPacked = pack_piece(captured);
  const int c_ti = (capPacked & 0x7) - 1;   // 0..5
  const int c_ci = (capPacked >> 3) & 0x1;  // 0/1
  assert(c_ti >= 0 && c_ti < 6);

  const bb::Bitboard fromBB = bb::sq_bb(from);
  const bb::Bitboard capBB = bb::sq_bb(capSq);
  const bb::Bitboard toBB = bb::sq_bb(to);

  // Preconditions on squares
  // Normal capture: capSq == to (occupied by captured)
  // En passant:     capSq != to and 'to' is empty before move
  if (capSq == to) {
    // Ensure 'to' currently holds the captured (not strictly required in release)
    // assert(m_piece_on[st] && "capture target must be occupied");
  } else {
    assert(m_piece_on[st] == 0 && "EP target square must be empty before the move");
  }

  // 1) Remove captured piece from its bitboards / occupancies / square
  m_bb[c_ci][c_ti] &= ~capBB;
  m_color_occ[c_ci] &= ~capBB;
  m_all_occ &= ~capBB;
  m_piece_on[sc] = 0;

  // 2) Move mover from -> to (bitboards & occupancies)
  m_bb[m_ci][m_ti] = (m_bb[m_ci][m_ti] & ~fromBB) | toBB;
  m_color_occ[m_ci] = (m_color_occ[m_ci] & ~fromBB) | toBB;
  m_all_occ = (m_all_occ & ~fromBB) | toBB;

  // 3) Update by-square
  m_piece_on[sf] = 0;
  m_piece_on[st] = moverPacked;
}

}  // namespace lilia::model
