#include "lilia/view/player_info_view.hpp"

#include <algorithm>  // std::clamp, std::min, std::max
#include <cmath>

#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view {

namespace {
// Palette in sync with the sidebar / start screen
const sf::Color kFrameFill(42, 48, 63);            // #2A303F
const sf::Color kFrameOutline(120, 140, 170, 60);  // subtle hairline
const sf::Color kNameColor(240, 244, 255);         // text
const sf::Color kEloColor(180, 186, 205);          // muted text

// Capture box fills (you wanted different tints per side)
const sf::Color kBoxDark(33, 38, 50);      // dark slot (for white player header)
const sf::Color kBoxLight(210, 215, 230);  // light slot (for black player header)

// Layout
constexpr float kIconFrameSize = 32.f;
constexpr float kIconOutline = 1.f;
constexpr float kTextGap = 12.f;  // gap between frame and name
constexpr float kEloGap = 6.f;    // gap between name and elo
constexpr float kCapPad = 4.f;    // inner padding inside capture box
constexpr float kCapMinH = 18.f;
constexpr float kCapMaxH = 28.f;        // keep compact in the row
constexpr float kPieceAdvance = 0.86f;  // horizontal advance factor (slight overlap)

inline float snapf(float v) {
  return std::round(v);
}
inline sf::Vector2f snap(sf::Vector2f p) {
  return {snapf(p.x), snapf(p.y)};
}
}  // namespace

PlayerInfoView::PlayerInfoView() {
  // 32x32 icon frame, 1px hairline border
  m_frame.setFillColor(kFrameFill);
  m_frame.setOutlineColor(kFrameOutline);
  m_frame.setOutlineThickness(kIconOutline);
  m_frame.setSize({kIconFrameSize, kIconFrameSize});

  if (m_font.loadFromFile(constant::STR_FILE_PATH_FONT)) {
    m_font.setSmooth(false);

    m_name.setFont(m_font);
    m_name.setCharacterSize(16);
    m_name.setFillColor(kNameColor);
    m_name.setStyle(sf::Text::Bold);

    m_elo.setFont(m_font);
    m_elo.setCharacterSize(15);
    m_elo.setFillColor(kEloColor);
    m_elo.setStyle(sf::Text::Regular);

    m_noCaptures.setFont(m_font);
    m_noCaptures.setCharacterSize(14);
    m_noCaptures.setString("no captures");
  }

  m_captureBox.setOutlineThickness(1.f);
  m_captureBox.setOutlineColor(kFrameOutline);
}

void PlayerInfoView::setPlayerColor(core::Color color) {
  m_playerColor = color;
  if (m_playerColor == core::Color::White) {
    m_captureBox.setFillColor(kBoxLight);
    m_noCaptures.setFillColor(kFrameFill);
  } else {
    m_captureBox.setFillColor(kBoxDark);
    m_noCaptures.setFillColor(kEloColor);
  }
}

void PlayerInfoView::setInfo(const PlayerInfo& info) {
  m_icon.setTexture(TextureTable::getInstance().get(info.iconPath));

  // scale icon to fit inside 32x32 frame with a small inner padding
  const auto size = m_icon.getOriginalSize();
  if (size.x > 0.f && size.y > 0.f) {
    const float innerPad = 2.f;
    const float targetW = kIconFrameSize - 2.f * innerPad;
    const float targetH = kIconFrameSize - 2.f * innerPad;
    const float sx = targetW / size.x;
    const float sy = targetH / size.y;
    const float s = std::min(sx, sy);
    m_icon.setScale(s, s);  // exact fit; no overscale
  }
  m_icon.setOriginToCenter();

  // text
  m_name.setString(info.name);
  if (info.elo == 0) {
    m_elo.setString("");
  } else {
    m_elo.setString(" (" + std::to_string(info.elo) + ")");
  }

  // captured pieces get re-laid out when we set position
}

void PlayerInfoView::setPosition(const Entity::Position& pos) {
  m_position = pos;

  // frame
  m_frame.setPosition(snap({pos.x, pos.y}));

  // icon centered in frame
  m_icon.setPosition(snap({pos.x + kIconFrameSize * 0.5f, pos.y + kIconFrameSize * 0.5f}));

  // name baseline aligned & vertically centered to frame
  auto nb = m_name.getLocalBounds();
  const float nameBaseY = pos.y + (kIconFrameSize - nb.height) * 0.5f - nb.top;
  const float textLeft = pos.x + kIconFrameSize + kTextGap;
  m_name.setPosition(snap({textLeft, nameBaseY}));

  // ELO follows name
  auto nB = m_name.getLocalBounds();
  const float eloX = textLeft + nB.width + kEloGap;
  m_elo.setPosition(snap({eloX, nameBaseY}));

  layoutCaptured();
}

void PlayerInfoView::setPositionClamped(const Entity::Position& pos,
                                        const sf::Vector2u& viewportSize) {
  const float outerW = kIconFrameSize + 2.f * kIconOutline;
  const float outerH = kIconFrameSize + 2.f * kIconOutline;

  const float pad = 8.f;
  Entity::Position clamped = pos;
  clamped.x = std::clamp(clamped.x, pad, static_cast<float>(viewportSize.x) - outerW - pad);
  clamped.y = std::clamp(clamped.y, pad, static_cast<float>(viewportSize.y) - outerH - pad);

  setPosition(clamped);
}

void PlayerInfoView::render(sf::RenderWindow& window) {
  window.draw(m_frame);
  m_icon.draw(window);
  window.draw(m_name);
  window.draw(m_elo);
  window.draw(m_captureBox);

  if (m_capturedPieces.empty()) {
    window.draw(m_noCaptures);
  } else {
    for (auto& piece : m_capturedPieces) {
      piece.draw(window);
    }
  }
}

void PlayerInfoView::addCapturedPiece(core::PieceType type, core::Color color) {
  // build piece texture path
  std::uint8_t numTypes = 6;
  std::string filename = constant::ASSET_PIECES_FILE_PATH + "/piece_" +
                         std::to_string(static_cast<std::uint8_t>(type) +
                                        numTypes * static_cast<std::uint8_t>(color)) +
                         ".png";
  const sf::Texture& texture = TextureTable::getInstance().get(filename);

  Entity piece(texture);
  piece.setScale(1.f, 1.f);
  m_capturedPieces.push_back(std::move(piece));
  layoutCaptured();
}

void PlayerInfoView::removeCapturedPiece() {
  if (!m_capturedPieces.empty()) {
    m_capturedPieces.pop_back();
    layoutCaptured();
  }
}

void PlayerInfoView::clearCapturedPieces() {
  m_capturedPieces.clear();
  layoutCaptured();
}

// --- Core layout logic: pieces fit inside the box height; box resizes to content.
void PlayerInfoView::layoutCaptured() {
  // Compute capture row height, centered in the 32px frame
  const float capH = std::clamp(kIconFrameSize - 6.f, kCapMinH, kCapMaxH);

  // Compute left edge = after name & ELO
  auto nameG = m_name.getGlobalBounds();
  auto eloG = m_elo.getGlobalBounds();
  const float rightText = std::max(nameG.left + nameG.width, eloG.left + eloG.width);
  const float baseX = snapf(rightText + kTextGap);
  const float baseY = snapf(m_frame.getPosition().y + (kIconFrameSize - capH) * 0.5f);

  if (m_capturedPieces.empty()) {
    // Text-centered capture box
    auto tb = m_noCaptures.getLocalBounds();
    const float boxW = tb.width + 2.f * kCapPad;
    m_captureBox.setSize({boxW, capH});
    m_captureBox.setPosition({baseX, baseY});

    // center text vertically, left padding horizontally
    const float tx = baseX + kCapPad;
    const float ty = baseY + (capH - tb.height) * 0.5f - tb.top;
    m_noCaptures.setPosition(snap({tx, ty}));
    return;
  }

  // Scale each piece to fit capH with padding, then place them L→R with slight overlap
  const float targetH = capH - 2.f * kCapPad;
  float x = kCapPad;

  for (auto& piece : m_capturedPieces) {
    const auto orig = piece.getOriginalSize();
    if (orig.x <= 0.f || orig.y <= 0.f) continue;

    const float s = (targetH / orig.y) * 1.3f;  // fit height
    piece.setScale(s, s);

    const float w = orig.x * s;
    const float h = orig.y * s;

    const float px = baseX + x;
    const float py = baseY + (capH - h) * 0.5f;
    piece.setPosition(snap({px, py}));
    x += w * kPieceAdvance;  // advance slightly less than full width to tuck them
  }

  // Right padding so last piece isn't flush
  const float contentW = x + kCapPad;
  m_captureBox.setSize({contentW, capH});
  m_captureBox.setPosition({baseX, baseY});
}

}  // namespace lilia::view
