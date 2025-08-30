#include "lilia/view/eval_bar.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <algorithm>
#include <cmath>

#include "lilia/model/position.hpp"
#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view {

EvalBar::EvalBar() : EvalBar::Entity() {
  setTexture(TextureTable::getInstance().get(constant::STR_TEXTURE_TRANSPARENT));
  setScale(constant::EVAL_BAR_WIDTH, constant::EVAL_BAR_HEIGHT);
  setOriginToCenter();
  m_black_background.setTexture(TextureTable::getInstance().get(constant::STR_TEXTURE_EVAL_WHITE));
  m_white_fill_eval.setTexture(TextureTable::getInstance().get(constant::STR_TEXTURE_EVAL_BLACK));
  m_black_background.setScale(constant::EVAL_BAR_WIDTH, constant::EVAL_BAR_HEIGHT);
  m_white_fill_eval.setScale(constant::EVAL_BAR_WIDTH, constant::EVAL_BAR_HEIGHT);
  m_black_background.setOriginToCenter();
  m_white_fill_eval.setOriginToCenter();
}

void EvalBar::setPosition(const Entity::Position& pos) {
  Entity::setPosition(pos);
  m_black_background.setPosition(getPosition());
  m_white_fill_eval.setPosition(getPosition());
}

void EvalBar::render(sf::RenderWindow& window) {
  draw(window);
  m_black_background.draw(window);
  m_white_fill_eval.draw(window);
}
void EvalBar::update(int eval) {
  scaleToEval(eval);
}

static float evalToWhitePct(int cp) {
  constexpr float k = 300.0f;              // chess.com-ähnliche Sättigung
  return 0.5f + 0.5f * std::tanh(cp / k);  // 0.5 = ausgeglichen
}

void EvalBar::scaleToEval(int e) {
  const float H = static_cast<float>(constant::EVAL_BAR_HEIGHT);
  const float W = static_cast<float>(constant::EVAL_BAR_WIDTH);

  const float pctWhite = evalToWhitePct(static_cast<float>(e));
  const float whitePx = std::clamp(pctWhite * H, 0.0f, H);

  // Sicherstellen, dass wir die Original-Texturgröße kennen
  auto whiteOrig = m_white_fill_eval.getOriginalSize();
  if (whiteOrig.x <= 0.f || whiteOrig.y <= 0.f) return;

  // Absolutgröße in Pixel => Skalierungsfaktoren = gewünschtePixel / OriginalPixel
  const float sx = W / whiteOrig.x;
  const float sy = whitePx / whiteOrig.y;
  m_white_fill_eval.setScale(sx, sy);

  // Weiß unten „anheften“, damit 50% exakt Mitte ist (Origin = Center)
  const auto p = getPosition();
  m_white_fill_eval.setPosition(Entity::Position{p.x, p.y + (H - whitePx) * 0.5f});

  // (Optional) Hintergrund sicher auf volle Größe bringen – einmalig im Ctor reicht,
  // aber falls du es hier robust machen willst:
  auto bgOrig = m_black_background.getOriginalSize();
  if (bgOrig.x > 0.f && bgOrig.y > 0.f) {
    m_black_background.setScale(W / bgOrig.x, H / bgOrig.y);
    m_black_background.setPosition(p);
  }
}

}  // namespace lilia::view
