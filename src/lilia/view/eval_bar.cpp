#include "lilia/view/eval_bar.hpp"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Window/Mouse.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

#include "lilia/engine/config.hpp"
#include "lilia/model/position.hpp"
#include "lilia/view/render_constants.hpp"
#include "lilia/view/texture_table.hpp"

namespace {
const sf::Color colHeaderBG(42, 48, 63);
const sf::Color colHoverBG(58, 66, 84);
const sf::Color colBorder(120, 140, 170, 50);
const sf::Color colText(240, 244, 255);
const sf::Color colAccentHover(120, 205, 255);
}  // namespace

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
  m_font.setSmooth(false);
  m_score_text.setFont(m_font);
  m_score_text.setCharacterSize(constant::EVAL_BAR_FONT_SIZE);
  m_toggle_text.setFont(m_font);
  m_toggle_text.setCharacterSize(16);
  // Default evaluation is 0.0 (balanced), which appears on the white side,
  // so draw the text in black for better visibility.
  m_score_text.setFillColor(sf::Color::Black);
}

void EvalBar::setPosition(const Entity::Position &pos) {
  Entity::setPosition(pos);
  m_black_background.setPosition(getPosition());
  m_white_fill_eval.setPosition(getPosition());

  float btnW = static_cast<float>(constant::EVAL_BAR_WIDTH);
  float btnH = 26.f;
  float toggleY = pos.y + static_cast<float>(constant::WINDOW_PX_SIZE) / 2.f +
                  (static_cast<float>(constant::SIDE_MARGIN) - btnH) / 2.f;
  m_toggle_bounds =
      sf::FloatRect(pos.x - btnW / 2.f, toggleY, btnW, btnH);
}

void EvalBar::render(sf::RenderWindow &window) {
  if (m_visible) {
    draw(window);                     // base (transparent)
    m_black_background.draw(window);  // dark background
    m_white_fill_eval.draw(window);   // white fill (scaled to eval)

    // --- hairline frame to blend with the UI panels ---
    const float W = static_cast<float>(constant::EVAL_BAR_WIDTH);
    const float H = static_cast<float>(constant::EVAL_BAR_HEIGHT);
    const float left = std::round(getPosition().x - W * 0.5f);
    const float top = std::round(getPosition().y - H * 0.5f);

    sf::RectangleShape frame({W, H});
    frame.setPosition(left, top);
    frame.setFillColor(sf::Color::Transparent);
    frame.setOutlineThickness(1.f);
    frame.setOutlineColor(sf::Color(120, 140, 170, 60));  // same hairline we used in sidebar
    window.draw(frame);

    window.draw(m_score_text);
  }

  // toggle button always rendered
  sf::Vector2i mp = sf::Mouse::getPosition(window);
  sf::Vector2f mpos = window.mapPixelToCoords(mp);
  bool hov = m_toggle_bounds.contains(mpos.x, mpos.y);

  sf::RectangleShape bg({m_toggle_bounds.width, m_toggle_bounds.height});
  bg.setPosition(m_toggle_bounds.left, m_toggle_bounds.top);
  bg.setFillColor(hov ? colHoverBG : colHeaderBG);
  bg.setOutlineThickness(1.f);
  bg.setOutlineColor(hov ? sf::Color(140, 200, 240, 90) : colBorder);
  window.draw(bg);

  m_toggle_text.setString(m_visible ? "Hide" : "Show");
  m_toggle_text.setFillColor(hov ? colAccentHover : colText);
  auto tb = m_toggle_text.getLocalBounds();
  m_toggle_text.setOrigin(tb.left + tb.width / 2.f, tb.top + tb.height / 2.f);
  m_toggle_text.setPosition(m_toggle_bounds.left + m_toggle_bounds.width / 2.f,
                            m_toggle_bounds.top + m_toggle_bounds.height / 2.f);
  window.draw(m_toggle_text);
}
void EvalBar::update(int eval) {
  if (!m_has_result) {
    m_target_eval = static_cast<float>(eval);
    m_display_eval += (m_target_eval - m_display_eval) * 0.05f;
  }
  scaleToEval(m_display_eval);

  if (m_has_result) {
    m_score_text.setString(m_result);
  } else {
    int absEval = std::abs(static_cast<int>(m_display_eval));
    if (absEval >= engine::MATE_THR) {
      int moves = (engine::MATE - absEval) / 2;
      std::string prefix = "M";
      m_score_text.setString(prefix + std::to_string(moves));
    } else {
      double val = std::abs(m_display_eval / 100.0);
      std::ostringstream ss;
      ss.setf(std::ios::fixed);
      ss << std::setprecision(1) << val;
      m_score_text.setString(ss.str());
    }
  }
  // Recompute origin after updating the text string
  auto b = m_score_text.getLocalBounds();
  m_score_text.setOrigin(b.width / 2.f, b.height / 2.f);

  // Place the score text on the side that currently has the advantage.
  // If the evaluation favors White (>= 0), position the text at the bottom
  // (white side) and draw it in black for contrast. Otherwise, position it
  // at the top (black side) and draw it in white.
  const float offset = 10.f;  // small margin from the edge of the bar
  const float barHalfHeight = static_cast<float>(constant::EVAL_BAR_HEIGHT) / 2.f;

  float xPos = getPosition().x;
  float yPos = getPosition().y;
  if (m_display_eval >= 0.f) {
    m_score_text.setFillColor(sf::Color::Black);
    yPos += barHalfHeight - offset * 1.5;  // *1.5 because of font origin position
  } else {
    m_score_text.setFillColor(sf::Color::White);
    yPos -= barHalfHeight - offset;
  }
  // Round to avoid blurry text caused by subpixel positioning
  m_score_text.setPosition(std::round(xPos), std::round(yPos));
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

  // Absolutgröße in Pixel => Skalierungsfaktoren = gewünschtePixel /
  // OriginalPixel
  const float sx = W / whiteOrig.x;
  const float sy = whitePx / whiteOrig.y;
  m_white_fill_eval.setScale(sx, sy);

  // Weiß unten „anheften“, damit 50% exakt Mitte ist (Origin = Center)
  const auto p = getPosition();
  m_white_fill_eval.setPosition(Entity::Position{p.x, p.y + (H - whitePx) * 0.5f});

  // (Optional) Hintergrund sicher auf volle Größe bringen – einmalig im Ctor
  // reicht, aber falls du es hier robust machen willst:
  auto bgOrig = m_black_background.getOriginalSize();
  if (bgOrig.x > 0.f && bgOrig.y > 0.f) {
    m_black_background.setScale(W / bgOrig.x, H / bgOrig.y);
    m_black_background.setPosition(p);
  }
}

void EvalBar::setResult(const std::string &result) {
  m_has_result = true;
  m_result = result;
  if (result == "1-0") {
    m_display_eval = m_target_eval = static_cast<float>(engine::MATE);
  } else if (result == "0-1") {
    m_display_eval = m_target_eval = -static_cast<float>(engine::MATE);
  } else {
    m_display_eval = m_target_eval = 0.f;
  }
  update(static_cast<int>(m_display_eval));
}

void EvalBar::reset() {
  m_has_result = false;
  m_result.clear();
  m_display_eval = 0.f;
  m_target_eval = 0.f;
  m_score_text.setString("0.0");
  auto b = m_score_text.getLocalBounds();
  m_score_text.setOrigin(b.width / 2.f, b.height / 2.f);
  scaleToEval(0.f);
}

void EvalBar::toggleVisibility() { m_visible = !m_visible; }

bool EvalBar::isOnToggle(core::MousePos mousePos) const {
  return m_toggle_bounds.contains(static_cast<float>(mousePos.x),
                                  static_cast<float>(mousePos.y));
}

}  // namespace lilia::view
