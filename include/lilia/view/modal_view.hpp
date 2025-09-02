#pragma once
#include <math.h>

#include <SFML/Graphics.hpp>

namespace lilia::view {

// Minimal themed modal manager for GameView
class ModalView {
 public:
  ModalView();

  // Must be called once (or let it succeed if already loaded)
  void loadFont(const std::string& fontPath);

  // Resign
  void showResign(const sf::Vector2u& windowSize, sf::Vector2f centerOnBoard);
  void hideResign();
  bool isResignOpen() const;

  // Game over
  void showGameOver(const std::string& msg, sf::Vector2f centerOnBoard);
  void hideGameOver();
  bool isGameOverOpen() const;

  // Layout on resize (re-center if needed)
  void onResize(const sf::Vector2u& windowSize, sf::Vector2f boardCenter);

  // The façade can render particles between these two calls:
  //   drawOverlay() -> particles -> drawPanel()
  void drawOverlay(sf::RenderWindow& win) const;
  void drawPanel(sf::RenderWindow& win) const;

  // Hit tests
  bool hitResignYes(sf::Vector2f p) const;
  bool hitResignNo(sf::Vector2f p) const;
  bool hitNewBot(sf::Vector2f p) const;
  bool hitRematch(sf::Vector2f p) const;
  bool hitClose(sf::Vector2f p) const;

 private:
  // theme (kept local to avoid touching your constants)
  const sf::Color colPanel = sf::Color(36, 41, 54, 230);     // #242936
  const sf::Color colHeader = sf::Color(42, 48, 63, 255);    // #2A303F
  const sf::Color colBorder = sf::Color(120, 140, 170, 60);  // hairline
  const sf::Color colText = sf::Color(240, 244, 255);
  const sf::Color colMuted = sf::Color(180, 186, 205);
  const sf::Color colAccent = sf::Color(100, 190, 255);  // #64BEFF
  const sf::Color colOverlay = sf::Color(0, 0, 0, 120);

  // geometry
  sf::Vector2u m_windowSize{};
  sf::Vector2f m_boardCenter{};
  sf::RectangleShape m_btnClose;
  sf::Text m_lblClose;
  sf::FloatRect m_hitClose;

  // state
  bool m_openResign = false;
  bool m_openGameOver = false;

  // visuals
  sf::Font m_font;
  sf::RectangleShape m_panel;    // body
  sf::RectangleShape m_border;   // 1px hairline
  sf::RectangleShape m_overlay;  // screen dim

  // text
  sf::Text m_title;
  sf::Text m_msg;

  // buttons (rectangles + labels)
  sf::RectangleShape m_btnLeft, m_btnRight;
  sf::Text m_lblLeft, m_lblRight;

  // cached hit areas
  sf::FloatRect m_hitLeft{}, m_hitRight{};

  // layout helpers
  void layoutCommon(sf::Vector2f center, sf::Vector2f panelSize);
  void stylePrimaryButton(sf::RectangleShape& btn, sf::Text& lbl);
  void styleSecondaryButton(sf::RectangleShape& btn, sf::Text& lbl);
  static inline float snapf(float v) { return std::round(v); }
};

}  // namespace lilia::view
