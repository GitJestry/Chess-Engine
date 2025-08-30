#include "lilia/view/eval_bar.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

#include "lilia/engine/config.hpp"
#include "lilia/model/position.hpp"
#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::view {

EvalBar::EvalBar() : EvalBar::Entity() {
  setTexture(TextureTable::getInstance().get(constant::STR_TEXTURE_TRANSPARENT));
  setScale(constant::EVAL_BAR_WIDTH, constant::EVAL_BAR_HEIGHT);
  setOriginToCenter();
  m_black_background.setTexture(TextureTable::getInstance().get(constant::STR_TEXTURE_EVAL_BLACK));
  m_white_fill_eval.setTexture(TextureTable::getInstance().get(constant::STR_TEXTURE_EVAL_WHITE));
  m_black_background.setScale(constant::EVAL_BAR_WIDTH, constant::EVAL_BAR_HEIGHT);
  m_white_fill_eval.setScale(constant::EVAL_BAR_WIDTH, constant::EVAL_BAR_HEIGHT);
  m_black_background.setOriginToCenter();
  m_white_fill_eval.setOriginToCenter();
  m_font.loadFromFile(constant::STR_FILE_PATH_FONT);
  m_score_text.setFont(m_font);
  m_score_text.setCharacterSize(18);
  // Default evaluation is 0.0 (balanced), which appears on the white side,
  // so draw the text in black for better visibility.
  m_score_text.setFillColor(sf::Color::Black);
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
  window.draw(m_score_text);
}
void EvalBar::update(int eval) {
  m_target_eval = static_cast<float>(eval);
  m_display_eval += (m_target_eval - m_display_eval) * 0.1f;
  scaleToEval(m_display_eval);

  int absEval = std::abs(eval);
  if (absEval >= engine::MATE_THR) {
    int moves = (engine::MATE - absEval) / 2;
    std::string prefix = eval > 0 ? "M" : "-M";
    m_score_text.setString(prefix + std::to_string(moves));
  } else {
    double val = m_display_eval / 100.0;
    std::ostringstream ss;
    ss.setf(std::ios::fixed); ss<< std::showpos << std::setprecision(1) << val;
    m_score_text.setString(ss.str());
  }
  // Recompute origin after updating the text string
  auto b = m_score_text.getLocalBounds();
  m_score_text.setOrigin(b.width / 2.f, b.height / 2.f);

  // Place the score text on the side that currently has the advantage.
  // If the evaluation favors White (>= 0), position the text at the bottom
  // (white side) and draw it in black for contrast. Otherwise, position it
  // at the top (black side) and draw it in white.
  const float offset = 10.f;  // small margin from the edge of the bar
  const float barHalfHeight =
      static_cast<float>(constant::EVAL_BAR_HEIGHT) / 2.f;

  float xPos = getPosition().x;
  float yPos = getPosition().y;
  if (m_display_eval >= 0.f) {
    m_score_text.setFillColor(sf::Color::Black);
    yPos += barHalfHeight - offset;
  } else {
    m_score_text.setFillColor(sf::Color::White);
    yPos -= barHalfHeight - offset;
  }
  m_score_text.setPosition(xPos, yPos);
}

static float evalToWhitePct(float cp) {
  constexpr float k = 1000.0f;             // langsamere Sättigung
  return 0.5f + 0.5f * std::tanh(cp / k);  // 0.5 = ausgeglichen
}

void EvalBar::scaleToEval(float e) {
  const float H = static_cast<float>(constant::EVAL_BAR_HEIGHT);
  const float W = static_cast<float>(constant::EVAL_BAR_WIDTH);

  const float pctWhite = evalToWhitePct(e);
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
