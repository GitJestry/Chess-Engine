#include "lilia/view/highlight_manager.hpp"

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view {

HighlightManager::HighlightManager(const BoardView& boardRef)
    : m_board_view_ref(boardRef),
      m_hl_attack_squares(),
      m_hl_select_squares(),
      m_hl_hover_squares(),
      m_hl_premove_squares(),
      m_hl_rclick_squares(),
      m_hl_rclick_arrows() {}

void HighlightManager::renderEntitiesToBoard(std::unordered_map<core::Square, Entity>& map,
                                             sf::RenderWindow& window) {
  for (auto& pair : map) {
    const auto& pos = pair.first;
    auto& entity = pair.second;
    entity.setPosition(m_board_view_ref.getSquareScreenPos(pos));
    entity.draw(window);
  }
}

void HighlightManager::renderAttack(sf::RenderWindow& window) {
  renderEntitiesToBoard(m_hl_attack_squares, window);
}
void HighlightManager::renderHover(sf::RenderWindow& window) {
  renderEntitiesToBoard(m_hl_hover_squares, window);
}
void HighlightManager::renderSelect(sf::RenderWindow& window) {
  renderEntitiesToBoard(m_hl_select_squares, window);
}
void HighlightManager::renderPremove(sf::RenderWindow& window) {
  renderEntitiesToBoard(m_hl_premove_squares, window);
}
void HighlightManager::renderRightClickSquares(sf::RenderWindow& window) {
  renderEntitiesToBoard(m_hl_rclick_squares, window);
}
void HighlightManager::renderRightClickArrows(sf::RenderWindow& window) {
  const sf::Color col(255, 80, 80, 170);
  const float sqSize = static_cast<float>(constant::SQUARE_PX_SIZE);
  const float thickness = sqSize * 0.15f;
  const float headLength = sqSize * 0.45f;
  const float headWidth = sqSize * 0.45f;
  const float edgeOffset = sqSize * 0.5f * 0.9f;

  auto drawSegment = [&](sf::Vector2f s, sf::Vector2f e, bool arrowHead) {
    sf::Vector2f diff = e - s;
    float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    if (len <= 0.1f) return;
    float angle = std::atan2(diff.y, diff.x) * 180.f / std::numbers::pi_v<float>;
    float bodyLen = arrowHead ? std::max(0.f, len - headLength) : len;
    sf::RectangleShape body({bodyLen, thickness});
    body.setFillColor(col);
    body.setOrigin(0.f, thickness / 2.f);
    body.setPosition(s);
    body.setRotation(angle);
    window.draw(body);

    if (arrowHead) {
      sf::ConvexShape head(3);
      head.setPoint(0, {0.f, 0.f});
      head.setPoint(1, {-headLength, headWidth / 2.f});
      head.setPoint(2, {-headLength, -headWidth / 2.f});
      head.setFillColor(col);
      head.setPosition(e);
      head.setRotation(angle);
      window.draw(head);
    }
  };

  for (const auto& kv : m_hl_rclick_arrows) {
    core::Square fromSq = kv.second.first;
    core::Square toSq = kv.second.second;
    auto fromPos = m_board_view_ref.getSquareScreenPos(fromSq);
    auto toPos = m_board_view_ref.getSquareScreenPos(toSq);

    int fx = static_cast<int>(fromSq) & 7;
    int fy = static_cast<int>(fromSq) >> 3;
    int tx = static_cast<int>(toSq) & 7;
    int ty = static_cast<int>(toSq) >> 3;
    int adx = std::abs(tx - fx);
    int ady = std::abs(ty - fy);
    bool knight = (adx == 1 && ady == 2) || (adx == 2 && ady == 1);

    if (knight) {
      int cornerFile = (ady > adx) ? fx : tx;
      int cornerRank = (ady > adx) ? ty : fy;
      core::Square cornerSq = static_cast<core::Square>(
          cornerFile + cornerRank * constant::BOARD_SIZE);
      sf::Vector2f corner = m_board_view_ref.getSquareScreenPos(cornerSq);

      int dx1 = cornerFile - fx;
      int dy1 = cornerRank - fy;
      sf::Vector2f start = fromPos;
      start.x += (dx1 > 0 ? edgeOffset : dx1 < 0 ? -edgeOffset : 0.f);
      start.y += (dy1 > 0 ? edgeOffset : dy1 < 0 ? -edgeOffset : 0.f);

      drawSegment(start, corner, false);
      drawSegment(corner, toPos, true);
      sf::CircleShape joint(thickness / 2.f);
      joint.setOrigin(thickness / 2.f, thickness / 2.f);
      joint.setFillColor(col);
      joint.setPosition(corner);
      window.draw(joint);
    } else {
      sf::Vector2f start = fromPos;
      int sgnX = (tx > fx) ? 1 : (tx < fx ? -1 : 0);
      int sgnY = (ty > fy) ? 1 : (ty < fy ? -1 : 0);
      start.x += sgnX * edgeOffset;
      start.y += sgnY * edgeOffset;
      drawSegment(start, toPos, true);
    }
  }
}

void HighlightManager::highlightSquare(core::Square pos) {
  Entity newSelectHlight(TextureTable::getInstance().get(constant::STR_TEXTURE_SELECTHLIGHT));
  newSelectHlight.setScale(constant::SQUARE_PX_SIZE, constant::SQUARE_PX_SIZE);
  m_hl_select_squares[pos] = std::move(newSelectHlight);
}
void HighlightManager::highlightAttackSquare(core::Square pos) {
  Entity newAttackHlight(TextureTable::getInstance().get(constant::STR_TEXTURE_ATTACKHLIGHT));
  m_hl_attack_squares[pos] = std::move(newAttackHlight);
}
void HighlightManager::highlightCaptureSquare(core::Square pos) {
  Entity newCaptureHlight(TextureTable::getInstance().get(constant::STR_TEXTURE_CAPTUREHLIGHT));
  m_hl_attack_squares[pos] = std::move(newCaptureHlight);
}

void HighlightManager::highlightHoverSquare(core::Square pos) {
  Entity newHoverHlight(TextureTable::getInstance().get(constant::STR_TEXTURE_HOVERHLIGHT));
  m_hl_hover_squares[pos] = std::move(newHoverHlight);
}
void HighlightManager::highlightPremoveSquare(core::Square pos) {
  Entity newPremove(TextureTable::getInstance().get(constant::STR_TEXTURE_PREMOVEHLIGHT));
  newPremove.setScale(constant::SQUARE_PX_SIZE, constant::SQUARE_PX_SIZE);
  m_hl_premove_squares[pos] = std::move(newPremove);
}
void HighlightManager::highlightRightClickSquare(core::Square pos) {
  if (auto it = m_hl_rclick_squares.find(pos); it != m_hl_rclick_squares.end()) {
    m_hl_rclick_squares.erase(it);
    return;
  }

  Entity newRC(TextureTable::getInstance().get(constant::STR_TEXTURE_RCLICKHLIGHT));
  newRC.setScale(constant::SQUARE_PX_SIZE, constant::SQUARE_PX_SIZE);
  m_hl_rclick_squares[pos] = std::move(newRC);
}

static unsigned int arrowKey(core::Square from, core::Square to) {
  return static_cast<unsigned int>(from) | (static_cast<unsigned int>(to) << 7);
}

void HighlightManager::highlightRightClickArrow(core::Square from, core::Square to) {
  unsigned int key = arrowKey(from, to);
  if (auto it = m_hl_rclick_arrows.find(key); it != m_hl_rclick_arrows.end()) {
    m_hl_rclick_arrows.erase(it);
    return;
  }
  m_hl_rclick_arrows[key] = {from, to};
}

std::vector<core::Square> HighlightManager::getRightClickSquares() const {
  std::vector<core::Square> out;
  out.reserve(m_hl_rclick_squares.size());
  for (const auto& kv : m_hl_rclick_squares) out.push_back(kv.first);
  return out;
}

std::vector<std::pair<core::Square, core::Square>>
HighlightManager::getRightClickArrows() const {
  std::vector<std::pair<core::Square, core::Square>> out;
  out.reserve(m_hl_rclick_arrows.size());
  for (const auto& kv : m_hl_rclick_arrows) out.push_back(kv.second);
  return out;
}
void HighlightManager::clearAllHighlights() {
  m_hl_select_squares.clear();
  m_hl_attack_squares.clear();
  m_hl_hover_squares.clear();
  m_hl_premove_squares.clear();
  m_hl_rclick_squares.clear();
  m_hl_rclick_arrows.clear();
}
void HighlightManager::clearNonPremoveHighlights() {
  m_hl_select_squares.clear();
  m_hl_attack_squares.clear();
  m_hl_hover_squares.clear();
  m_hl_rclick_squares.clear();
  m_hl_rclick_arrows.clear();
}
void HighlightManager::clearAttackHighlights() { m_hl_attack_squares.clear(); }
void HighlightManager::clearHighlightSquare(core::Square pos) {
  m_hl_select_squares.erase(pos);
}
void HighlightManager::clearHighlightHoverSquare(core::Square pos) {
  m_hl_hover_squares.erase(pos);
}
void HighlightManager::clearHighlightPremoveSquare(core::Square pos) {
  m_hl_premove_squares.erase(pos);
}
void HighlightManager::clearPremoveHighlights() {
  m_hl_premove_squares.clear();
}
void HighlightManager::clearRightClickHighlights() {
  m_hl_rclick_squares.clear();
  m_hl_rclick_arrows.clear();
}

}  // namespace lilia::view
