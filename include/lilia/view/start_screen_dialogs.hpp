#pragma once

#include <SFML/Graphics.hpp>
#include <string>

namespace lilia::view {

class LoadGameModal {
 public:
  explicit LoadGameModal(const sf::Font &font);

  void open(const std::string &fen, const std::string &pgn);
  void close();
  [[nodiscard]] bool isOpen() const;

  enum class EventResult { None, Closed };
  EventResult handleEvent(const sf::Event &event, sf::Vector2f mousePos);

  void draw(sf::RenderWindow &window);
  void applyTheme();

  [[nodiscard]] const std::string &getFen() const { return m_fenString; }
  [[nodiscard]] const std::string &getPgn() const { return m_pgnString; }

 private:
  void layout(const sf::Vector2u &windowSize);
  void refreshTexts();
  void resetCaret();
  void updateCaretVisibility();
  bool handleTextEntered(sf::Uint32 unicode);
  bool handleKeyPressed(const sf::Event::KeyEvent &key, bool ctrlDown);
  void pasteFromClipboard();
  void drawCaret(sf::RenderWindow &window) const;

  const sf::Font &m_font;
  bool m_visible{false};

  sf::RectangleShape m_overlay;
  sf::RectangleShape m_panel;
  sf::Text m_title;
  sf::Text m_fenLabel;
  sf::RectangleShape m_fenBox;
  sf::Text m_fenText;
  sf::Text m_fenPlaceholder;
  sf::Text m_pgnLabel;
  sf::RectangleShape m_pgnBox;
  sf::Text m_pgnText;
  sf::Text m_pgnPlaceholder;
  sf::RectangleShape m_closeButton;
  sf::Text m_closeText;

  std::string m_fenString;
  std::string m_pgnString;

  bool m_fenActive{false};
  bool m_pgnActive{false};
  mutable sf::Clock m_caretClock;
  float m_fenCaretX{0.f};
  float m_pgnCaretX{0.f};
  float m_pgnCaretY{0.f};
};

class WarningDialog {
 public:
  explicit WarningDialog(const sf::Font &font);

  void open(const std::string &message);
  void close();
  [[nodiscard]] bool isOpen() const;

  enum class Choice { None, UseDefaults, Cancel };
  Choice handleEvent(const sf::Event &event, sf::Vector2f mousePos);

  void draw(sf::RenderWindow &window);
  void applyTheme();

 private:
  void layout(const sf::Vector2u &windowSize);

  const sf::Font &m_font;
  bool m_visible{false};

  sf::RectangleShape m_overlay;
  sf::RectangleShape m_panel;
  sf::Text m_message;
  sf::RectangleShape m_acceptBtn;
  sf::RectangleShape m_cancelBtn;
  sf::Text m_acceptText;
  sf::Text m_cancelText;
};

}  // namespace lilia::view
