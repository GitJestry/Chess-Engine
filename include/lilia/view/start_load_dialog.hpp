#pragma once

#include <SFML/Graphics.hpp>

#include <string>

#include "lilia/view/start_screen_ui.hpp"

namespace lilia::view {

enum class StartLoadDialogResult { None, Applied, Cancelled };

class StartLoadDialog {
 public:
  explicit StartLoadDialog(sf::Font& font);

  void open(const sf::Vector2u& windowSize, const sf::FloatRect& anchorPanel);
  void close();
  bool isOpen() const { return m_visible; }

  StartLoadDialogResult handleEvent(const sf::Event& e, const sf::Vector2f& mousePos);
  void draw(sf::RenderWindow& window) const;
  void updateLayout(const sf::Vector2u& windowSize, const sf::FloatRect& anchorPanel);

  void applyTheme();

  void setFen(const std::string& fen);
  void setPgn(const std::string& pgn);

  const std::string& fen() const { return m_fenField.value; }
  const std::string& pgn() const { return m_pgnField.value; }

  bool fenValid() const;
  bool pgnValid() const;

 private:
  struct InputField {
    sf::RectangleShape box;
    sf::Text text;
    sf::Text placeholder;
    std::string value;
    bool active{false};
    bool multiline{false};
    float padX{8.f};
    float padY{6.f};
  };

  void layout(const sf::Vector2u& windowSize, const sf::FloatRect& anchorPanel);
  void updateText(InputField& field) const;
  void drawField(sf::RenderWindow& window, const InputField& field, bool valid) const;
  void handleTextInput(InputField& field, sf::Uint32 unicode);
  void handlePaste(InputField& field);
  void refreshFieldStyles();

  sf::Font& m_font;
  bool m_visible{false};

  sf::RectangleShape m_overlay;
  sf::RectangleShape m_panel;
  sf::Text m_title;
  sf::Text m_description;
  sf::Text m_fenLabel;
  sf::Text m_pgnLabel;
  sf::Text m_hintText;

  InputField m_fenField;
  InputField m_pgnField;

  sf::RectangleShape m_cancelBtn;
  sf::RectangleShape m_applyBtn;
  sf::Text m_cancelText;
  sf::Text m_applyText;

  sf::Vector2f m_lastMousePos{0.f, 0.f};
  bool m_cancelPressed{false};
  bool m_applyPressed{false};
  sf::Clock m_caretClock;
};

}  // namespace lilia::view

