#include "lilia/view/start_screen_dialogs.hpp"

#include <SFML/Window/Clipboard.hpp>
#include <algorithm>

#include "lilia/view/color_palette_manager.hpp"

namespace lilia::view {

namespace {
constexpr float kPanelWidth = 520.f;
constexpr float kPanelHeight = 420.f;
constexpr float kFieldPad = 14.f;
constexpr float kLineHeight = 24.f;
constexpr float kCaretWidth = 2.f;
constexpr float kPlaceholderAlpha = 120.f;

sf::Color overlayColor() {
  sf::Color c = ColorPaletteManager::get().palette().COL_BG_BOTTOM;
  c.a = 160;
  return c;
}

sf::Color panelColor() {
  return ColorPaletteManager::get().palette().COL_PANEL;
}

sf::Color borderColor() {
  return ColorPaletteManager::get().palette().COL_PANEL_BORDER_ALT;
}

sf::Color inputColor() {
  return ColorPaletteManager::get().palette().COL_INPUT_BG;
}

sf::Color inputBorderColor() {
  return ColorPaletteManager::get().palette().COL_INPUT_BORDER;
}

sf::Color textColor() {
  return ColorPaletteManager::get().palette().COL_TEXT;
}

sf::Color subtleTextColor() {
  return ColorPaletteManager::get().palette().COL_MUTED_TEXT;
}

sf::Color accentColor() {
  return ColorPaletteManager::get().palette().COL_ACCENT;
}

float measureTextWidth(const sf::Text &text) {
  return text.getLocalBounds().width;
}

}  // namespace

LoadGameModal::LoadGameModal(const sf::Font &font) : m_font(font) {
  m_overlay.setFillColor(overlayColor());
  m_panel.setSize({kPanelWidth, kPanelHeight});
  m_panel.setFillColor(panelColor());
  m_panel.setOutlineThickness(2.f);
  m_panel.setOutlineColor(borderColor());

  m_title.setFont(m_font);
  m_title.setCharacterSize(26);
  m_title.setString("Load Game");
  m_title.setFillColor(textColor());

  m_fenLabel.setFont(m_font);
  m_fenLabel.setCharacterSize(16);
  m_fenLabel.setString("FEN");
  m_fenLabel.setFillColor(subtleTextColor());

  m_pgnLabel = m_fenLabel;
  m_pgnLabel.setString("PGN");

  m_fenText.setFont(m_font);
  m_fenText.setCharacterSize(16);
  m_fenText.setFillColor(textColor());

  m_pgnText.setFont(m_font);
  m_pgnText.setCharacterSize(15);
  m_pgnText.setFillColor(textColor());

  m_fenPlaceholder = m_fenText;
  m_fenPlaceholder.setFillColor(sf::Color(textColor().r, textColor().g, textColor().b, 110));
  m_fenPlaceholder.setString("Leave empty to use standard start");

  m_pgnPlaceholder = m_pgnText;
  m_pgnPlaceholder.setFillColor(sf::Color(textColor().r, textColor().g, textColor().b, 110));
  m_pgnPlaceholder.setString("Paste PGN moves here");

  m_fenBox.setFillColor(inputColor());
  m_fenBox.setOutlineThickness(2.f);
  m_fenBox.setOutlineColor(inputBorderColor());

  m_pgnBox = m_fenBox;

  m_closeButton.setSize({32.f, 32.f});
  m_closeButton.setFillColor(sf::Color::Transparent);
  m_closeButton.setOutlineThickness(2.f);
  m_closeButton.setOutlineColor(subtleTextColor());

  m_closeText.setFont(m_font);
  m_closeText.setCharacterSize(20);
  m_closeText.setString("×");
  m_closeText.setFillColor(subtleTextColor());
}

void LoadGameModal::open(const std::string &fen, const std::string &pgn) {
  m_visible = true;
  m_fenString = fen;
  m_pgnString = pgn;
  m_fenActive = true;
  m_pgnActive = false;
  resetCaret();
}

void LoadGameModal::close() { m_visible = false; }

bool LoadGameModal::isOpen() const { return m_visible; }

void LoadGameModal::layout(const sf::Vector2u &windowSize) {
  m_overlay.setSize({static_cast<float>(windowSize.x), static_cast<float>(windowSize.y)});
  const sf::Vector2f center{static_cast<float>(windowSize.x) * 0.5f,
                            static_cast<float>(windowSize.y) * 0.5f};
  m_panel.setPosition(center.x - kPanelWidth * 0.5f, center.y - kPanelHeight * 0.5f);

  m_title.setPosition(m_panel.getPosition().x + kFieldPad,
                      m_panel.getPosition().y + kFieldPad);

  m_closeButton.setPosition(m_panel.getPosition().x + kPanelWidth - m_closeButton.getSize().x -
                                kFieldPad * 0.5f,
                            m_panel.getPosition().y + kFieldPad * 0.5f);
  auto closeBounds = m_closeText.getLocalBounds();
  m_closeText.setOrigin(closeBounds.left + closeBounds.width * 0.5f,
                        closeBounds.top + closeBounds.height * 0.5f);
  m_closeText.setPosition(m_closeButton.getPosition().x + m_closeButton.getSize().x * 0.5f,
                          m_closeButton.getPosition().y + m_closeButton.getSize().y * 0.5f);

  const float fieldLeft = m_panel.getPosition().x + kFieldPad;
  float cursorY = m_title.getPosition().y + 48.f;

  m_fenLabel.setPosition(fieldLeft, cursorY);
  cursorY += kLineHeight;
  m_fenBox.setPosition(fieldLeft, cursorY);
  m_fenBox.setSize({kPanelWidth - 2.f * kFieldPad, 38.f});
  cursorY += m_fenBox.getSize().y + kFieldPad;

  m_pgnLabel.setPosition(fieldLeft, cursorY);
  cursorY += kLineHeight;
  m_pgnBox.setPosition(fieldLeft, cursorY);
  m_pgnBox.setSize({kPanelWidth - 2.f * kFieldPad, kPanelHeight - (cursorY - m_panel.getPosition().y) -
                                             kFieldPad});

  refreshTexts();
}

void LoadGameModal::refreshTexts() {
  const float fenTextX = m_fenBox.getPosition().x + kFieldPad;
  const float fenTextY = m_fenBox.getPosition().y + m_fenBox.getSize().y * 0.5f - 8.f;
  m_fenText.setString(m_fenString);
  m_fenText.setPosition(fenTextX, fenTextY);
  m_fenPlaceholder.setPosition(fenTextX, fenTextY);

  const float pgnTextX = m_pgnBox.getPosition().x + kFieldPad;
  const float pgnTextY = m_pgnBox.getPosition().y + kFieldPad;
  m_pgnText.setString(m_pgnString);
  m_pgnText.setPosition(pgnTextX, pgnTextY);
  m_pgnPlaceholder.setPosition(pgnTextX, pgnTextY);

  m_fenCaretX = m_fenText.getPosition().x + measureTextWidth(m_fenText);
  const std::string &pgn = m_pgnString;
  std::size_t lastNewline = pgn.rfind('\n');
  std::string lastLine = (lastNewline == std::string::npos) ? pgn : pgn.substr(lastNewline + 1);
  sf::Text tmp(lastLine, m_font, m_pgnText.getCharacterSize());
  m_pgnCaretX = m_pgnText.getPosition().x + measureTextWidth(tmp);
  int lineCount = 0;
  for (char c : pgn) {
    if (c == '\n') ++lineCount;
  }
  m_pgnCaretY = m_pgnText.getPosition().y + static_cast<float>(lineCount) *
                                         (m_pgnText.getCharacterSize() + 4.f);
}

void LoadGameModal::resetCaret() {
  m_caretClock.restart();
  refreshTexts();
}

void LoadGameModal::updateCaretVisibility() {
  if (m_caretClock.getElapsedTime().asSeconds() > 0.8f) {
    m_caretClock.restart();
  }
}

bool LoadGameModal::handleTextEntered(sf::Uint32 unicode) {
  if (unicode == 8) {  // backspace
    if (m_fenActive) {
      if (!m_fenString.empty()) m_fenString.pop_back();
    } else if (m_pgnActive) {
      if (!m_pgnString.empty()) m_pgnString.pop_back();
    }
    resetCaret();
    return true;
  }
  if (unicode == 13) {  // enter
    if (m_pgnActive) {
      m_pgnString.push_back('\n');
      resetCaret();
      return true;
    }
    return false;
  }
  if (unicode < 32 || unicode > 126) return false;
  char c = static_cast<char>(unicode);
  if (m_fenActive) {
    std::string probe = m_fenString;
    probe.push_back(c);
    sf::Text tmp(probe, m_font, m_fenText.getCharacterSize());
    if (tmp.getLocalBounds().width <= m_fenBox.getSize().x - 2.f * kFieldPad) {
      m_fenString.push_back(c);
      resetCaret();
      return true;
    }
    return false;
  }
  if (m_pgnActive) {
    m_pgnString.push_back(c);
    resetCaret();
    return true;
  }
  return false;
}

bool LoadGameModal::handleKeyPressed(const sf::Event::KeyEvent &key, bool ctrlDown) {
  if (ctrlDown && key.code == sf::Keyboard::V) {
    pasteFromClipboard();
    return true;
  }
  if (key.code == sf::Keyboard::Tab) {
    if (m_fenActive) {
      m_fenActive = false;
      m_pgnActive = true;
    } else {
      m_fenActive = true;
      m_pgnActive = false;
    }
    resetCaret();
    return true;
  }
  if (key.code == sf::Keyboard::Escape) {
    m_visible = false;
    return true;
  }
  return false;
}

void LoadGameModal::pasteFromClipboard() {
  auto clip = sf::Clipboard::getString().toAnsiString();
  if (clip.empty()) return;
  if (m_fenActive) {
    std::string filtered;
    filtered.reserve(clip.size());
    for (char c : clip) {
      if (c == '\n' || c == '\r') continue;
      if (c >= 32 && c <= 126) filtered.push_back(c);
    }
    for (char c : filtered) {
      sf::Text tmp(m_fenString + c, m_font, m_fenText.getCharacterSize());
      if (tmp.getLocalBounds().width <= m_fenBox.getSize().x - 2.f * kFieldPad)
        m_fenString.push_back(c);
      else
        break;
    }
  } else if (m_pgnActive) {
    for (char c : clip) {
      if (c == '\r') continue;
      if (c == '\t') {
        m_pgnString.push_back(' ');
      } else {
        m_pgnString.push_back(c);
      }
    }
  }
  resetCaret();
}

LoadGameModal::EventResult LoadGameModal::handleEvent(const sf::Event &event,
                                                      sf::Vector2f mousePos) {
  if (!m_visible) return EventResult::None;
  if (event.type == sf::Event::MouseButtonPressed &&
      event.mouseButton.button == sf::Mouse::Left) {
    if (m_closeButton.getGlobalBounds().contains(mousePos)) {
      m_visible = false;
      return EventResult::Closed;
    }
    if (m_fenBox.getGlobalBounds().contains(mousePos)) {
      m_fenActive = true;
      m_pgnActive = false;
      resetCaret();
    } else if (m_pgnBox.getGlobalBounds().contains(mousePos)) {
      m_fenActive = false;
      m_pgnActive = true;
      resetCaret();
    }
  }
  if (event.type == sf::Event::TextEntered) {
    if (handleTextEntered(event.text.unicode)) return EventResult::None;
  }
  if (event.type == sf::Event::KeyPressed) {
    bool ctrl = event.key.control || event.key.system;
    if (handleKeyPressed(event.key, ctrl)) {
      if (!m_visible) return EventResult::Closed;
      return EventResult::None;
    }
  }
  updateCaretVisibility();
  return EventResult::None;
}

void LoadGameModal::drawCaret(sf::RenderWindow &window) const {
  if (!m_visible) return;
  if (m_caretClock.getElapsedTime().asSeconds() > 0.5f) return;
  sf::RectangleShape caret;
  caret.setFillColor(textColor());
  caret.setSize({kCaretWidth, m_fenBox.getSize().y - 12.f});
  if (m_fenActive) {
    caret.setPosition(m_fenCaretX, m_fenBox.getPosition().y + 6.f);
    window.draw(caret);
  } else if (m_pgnActive) {
    caret.setSize({kCaretWidth, static_cast<float>(m_pgnText.getCharacterSize() + 4)});
    caret.setPosition(m_pgnCaretX,
                      m_pgnCaretY);
    window.draw(caret);
  }
}

void LoadGameModal::draw(sf::RenderWindow &window) {
  if (!m_visible) return;
  layout(window.getSize());

  window.draw(m_overlay);
  window.draw(m_panel);
  window.draw(m_title);
  window.draw(m_closeButton);
  window.draw(m_closeText);
  window.draw(m_fenLabel);
  window.draw(m_fenBox);
  if (m_fenString.empty())
    window.draw(m_fenPlaceholder);
  else
    window.draw(m_fenText);
  window.draw(m_pgnLabel);
  window.draw(m_pgnBox);
  if (m_pgnString.empty())
    window.draw(m_pgnPlaceholder);
  else
    window.draw(m_pgnText);

  drawCaret(window);
}

void LoadGameModal::applyTheme() {
  m_overlay.setFillColor(overlayColor());
  m_panel.setFillColor(panelColor());
  m_panel.setOutlineColor(borderColor());
  m_fenBox.setFillColor(inputColor());
  m_fenBox.setOutlineColor(inputBorderColor());
  m_pgnBox.setFillColor(inputColor());
  m_pgnBox.setOutlineColor(inputBorderColor());
  m_title.setFillColor(textColor());
  m_fenLabel.setFillColor(subtleTextColor());
  m_pgnLabel.setFillColor(subtleTextColor());
  m_fenText.setFillColor(textColor());
  m_pgnText.setFillColor(textColor());
  m_fenPlaceholder.setFillColor(sf::Color(textColor().r, textColor().g, textColor().b, 110));
  m_pgnPlaceholder.setFillColor(sf::Color(textColor().r, textColor().g, textColor().b, 110));
  m_closeButton.setOutlineColor(subtleTextColor());
  m_closeText.setFillColor(subtleTextColor());
}

/* ---------------- WarningDialog ---------------- */

WarningDialog::WarningDialog(const sf::Font &font) : m_font(font) {
  m_overlay.setFillColor(overlayColor());
  m_panel.setSize({380.f, 180.f});
  m_panel.setFillColor(panelColor());
  m_panel.setOutlineThickness(2.f);
  m_panel.setOutlineColor(borderColor());

  m_message.setFont(m_font);
  m_message.setCharacterSize(18);
  m_message.setFillColor(textColor());

  m_acceptBtn.setSize({150.f, 40.f});
  m_acceptBtn.setFillColor(accentColor());
  m_acceptBtn.setOutlineThickness(0.f);

  m_cancelBtn = m_acceptBtn;
  m_cancelBtn.setFillColor(inputColor());
  m_cancelBtn.setOutlineThickness(2.f);
  m_cancelBtn.setOutlineColor(inputBorderColor());

  m_acceptText.setFont(m_font);
  m_acceptText.setCharacterSize(16);
  m_acceptText.setString("Use defaults");
  m_acceptText.setFillColor(ColorPaletteManager::get().palette().COL_DARK_TEXT);

  m_cancelText.setFont(m_font);
  m_cancelText.setCharacterSize(16);
  m_cancelText.setString("Back");
  m_cancelText.setFillColor(textColor());
}

void WarningDialog::open(const std::string &message) {
  m_visible = true;
  m_message.setString(message);
}

void WarningDialog::close() { m_visible = false; }

bool WarningDialog::isOpen() const { return m_visible; }

void WarningDialog::layout(const sf::Vector2u &windowSize) {
  m_overlay.setSize({static_cast<float>(windowSize.x), static_cast<float>(windowSize.y)});
  const sf::Vector2f center{static_cast<float>(windowSize.x) * 0.5f,
                            static_cast<float>(windowSize.y) * 0.5f};
  m_panel.setPosition(center.x - m_panel.getSize().x * 0.5f,
                      center.y - m_panel.getSize().y * 0.5f);

  sf::FloatRect msgBounds = m_message.getLocalBounds();
  m_message.setOrigin(msgBounds.left + msgBounds.width * 0.5f,
                      msgBounds.top + msgBounds.height * 0.5f);
  m_message.setPosition(center.x, m_panel.getPosition().y + 60.f);

  const float buttonY = m_panel.getPosition().y + m_panel.getSize().y - 60.f;
  const float gap = 20.f;
  m_acceptBtn.setPosition(center.x - m_acceptBtn.getSize().x - gap * 0.5f, buttonY);
  m_cancelBtn.setPosition(center.x + gap * 0.5f, buttonY);

  auto centerText = [](sf::Text &txt, const sf::RectangleShape &box) {
    auto b = txt.getLocalBounds();
    txt.setOrigin(b.left + b.width * 0.5f, b.top + b.height * 0.5f);
    txt.setPosition(box.getPosition().x + box.getSize().x * 0.5f,
                    box.getPosition().y + box.getSize().y * 0.5f);
  };

  centerText(m_acceptText, m_acceptBtn);
  centerText(m_cancelText, m_cancelBtn);
}

WarningDialog::Choice WarningDialog::handleEvent(const sf::Event &event,
                                                  sf::Vector2f mousePos) {
  if (!m_visible) return Choice::None;
  if (event.type == sf::Event::MouseButtonPressed &&
      event.mouseButton.button == sf::Mouse::Left) {
    if (m_acceptBtn.getGlobalBounds().contains(mousePos)) {
      m_visible = false;
      return Choice::UseDefaults;
    }
    if (m_cancelBtn.getGlobalBounds().contains(mousePos)) {
      m_visible = false;
      return Choice::Cancel;
    }
  }
  if (event.type == sf::Event::KeyPressed) {
    if (event.key.code == sf::Keyboard::Enter) {
      m_visible = false;
      return Choice::UseDefaults;
    }
    if (event.key.code == sf::Keyboard::Escape) {
      m_visible = false;
      return Choice::Cancel;
    }
  }
  return Choice::None;
}

void WarningDialog::draw(sf::RenderWindow &window) {
  if (!m_visible) return;
  layout(window.getSize());
  window.draw(m_overlay);
  window.draw(m_panel);
  window.draw(m_message);
  window.draw(m_acceptBtn);
  window.draw(m_cancelBtn);
  window.draw(m_acceptText);
  window.draw(m_cancelText);
}

void WarningDialog::applyTheme() {
  m_overlay.setFillColor(overlayColor());
  m_panel.setFillColor(panelColor());
  m_panel.setOutlineColor(borderColor());
  m_message.setFillColor(textColor());
  m_acceptBtn.setFillColor(accentColor());
  m_acceptText.setFillColor(ColorPaletteManager::get().palette().COL_DARK_TEXT);
  m_cancelBtn.setFillColor(inputColor());
  m_cancelBtn.setOutlineColor(inputBorderColor());
  m_cancelText.setFillColor(textColor());
}

}  // namespace lilia::view
