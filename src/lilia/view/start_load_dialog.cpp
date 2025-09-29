#include "lilia/view/start_load_dialog.hpp"

#include <SFML/Window/Clipboard.hpp>

#include <algorithm>
#include <cmath>

#include "lilia/constants.hpp"
#include "lilia/view/color_palette_manager.hpp"
#include "lilia/view/start_validation.hpp"

namespace lilia::view {

StartLoadDialog::StartLoadDialog(sf::Font& font) : m_font(font) {
  m_title.setFont(m_font);
  m_title.setString("Load Game State");
  m_title.setCharacterSize(24);

  m_description.setFont(m_font);
  m_description.setString("Paste a FEN or PGN to continue a game.");
  m_description.setCharacterSize(16);

  m_fenLabel.setFont(m_font);
  m_fenLabel.setString("FEN");
  m_fenLabel.setCharacterSize(15);

  m_pgnLabel.setFont(m_font);
  m_pgnLabel.setString("PGN");
  m_pgnLabel.setCharacterSize(15);

  m_hintText.setFont(m_font);
  m_hintText.setString("Leave FEN empty to start from the standard position.");
  m_hintText.setCharacterSize(13);

  m_fenField.text.setFont(m_font);
  m_fenField.text.setCharacterSize(18);
  m_fenField.placeholder.setFont(m_font);
  m_fenField.placeholder.setCharacterSize(18);
  m_fenField.placeholder.setString("Paste or type a FEN");
  m_fenField.box.setOutlineThickness(1.5f);

  m_pgnField.text.setFont(m_font);
  m_pgnField.text.setCharacterSize(16);
  m_pgnField.placeholder.setFont(m_font);
  m_pgnField.placeholder.setCharacterSize(16);
  m_pgnField.placeholder.setString("Paste or type PGN moves");
  m_pgnField.box.setOutlineThickness(1.5f);
  m_pgnField.multiline = true;
  m_pgnField.padY = 10.f;

  m_cancelText.setFont(m_font);
  m_cancelText.setCharacterSize(18);
  m_cancelText.setString("Cancel");

  m_applyText.setFont(m_font);
  m_applyText.setCharacterSize(18);
  m_applyText.setString("Apply");

  m_panel.setOutlineThickness(1.f);

  applyTheme();
  setFen("");
  setPgn("");
}

void StartLoadDialog::open(const sf::Vector2u& windowSize, const sf::FloatRect& anchorPanel) {
  m_visible = true;
  m_fenField.active = true;
  m_pgnField.active = false;
  m_caretClock.restart();
  layout(windowSize, anchorPanel);
  refreshFieldStyles();
}

void StartLoadDialog::close() {
  m_visible = false;
  m_fenField.active = false;
  m_pgnField.active = false;
  m_cancelPressed = false;
  m_applyPressed = false;
}

StartLoadDialogResult StartLoadDialog::handleEvent(const sf::Event& e, const sf::Vector2f& mousePos) {
  if (!m_visible) return StartLoadDialogResult::None;

  m_lastMousePos = mousePos;

  if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
    const sf::Vector2f mp((float)e.mouseButton.x, (float)e.mouseButton.y);
    if (!m_panel.getGlobalBounds().contains(mp)) {
      close();
      return StartLoadDialogResult::Cancelled;
    }

    m_cancelPressed = m_cancelBtn.getGlobalBounds().contains(mp);
    m_applyPressed = m_applyBtn.getGlobalBounds().contains(mp);

    if (m_fenField.box.getGlobalBounds().contains(mp)) {
      m_fenField.active = true;
      m_pgnField.active = false;
      m_caretClock.restart();
    } else if (m_pgnField.box.getGlobalBounds().contains(mp)) {
      m_fenField.active = false;
      m_pgnField.active = true;
      m_caretClock.restart();
    } else if (!m_cancelPressed && !m_applyPressed) {
      m_fenField.active = false;
      m_pgnField.active = false;
    }
  }

  if (e.type == sf::Event::MouseButtonReleased && e.mouseButton.button == sf::Mouse::Left) {
    const sf::Vector2f mp((float)e.mouseButton.x, (float)e.mouseButton.y);
    StartLoadDialogResult result = StartLoadDialogResult::None;
    if (m_cancelPressed && m_cancelBtn.getGlobalBounds().contains(mp)) {
      close();
      result = StartLoadDialogResult::Cancelled;
    } else if (m_applyPressed && m_applyBtn.getGlobalBounds().contains(mp)) {
      close();
      result = StartLoadDialogResult::Applied;
    }
    m_cancelPressed = false;
    m_applyPressed = false;
    refreshFieldStyles();
    return result;
  }

  if (e.type == sf::Event::KeyPressed) {
    if (e.key.code == sf::Keyboard::Escape) {
      close();
      return StartLoadDialogResult::Cancelled;
    }
    if ((e.key.control || e.key.system) && e.key.code == sf::Keyboard::V) {
      if (m_fenField.active) {
        handlePaste(m_fenField);
        refreshFieldStyles();
      } else if (m_pgnField.active) {
        handlePaste(m_pgnField);
        refreshFieldStyles();
      }
    } else if (e.key.code == sf::Keyboard::Enter) {
      // Allow newline in PGN when not using modifiers.
      if (!(m_pgnField.active && !e.key.control && !e.key.alt && !e.key.system && !e.key.shift)) {
        close();
        return StartLoadDialogResult::Applied;
      }
    }
  }

  if (e.type == sf::Event::TextEntered) {
    if (m_fenField.active) {
      handleTextInput(m_fenField, e.text.unicode);
      refreshFieldStyles();
    } else if (m_pgnField.active) {
      handleTextInput(m_pgnField, e.text.unicode);
      refreshFieldStyles();
    }
  }

  refreshFieldStyles();
  return StartLoadDialogResult::None;
}

void StartLoadDialog::draw(sf::RenderWindow& window) const {
  if (!m_visible) return;

  window.draw(m_overlay);
  window.draw(m_panel);

  window.draw(m_title);
  window.draw(m_description);
  window.draw(m_fenLabel);
  window.draw(m_hintText);
  window.draw(m_pgnLabel);

  const bool fenValidNow = fenValid();
  const bool pgnValidNow = pgnValid();

  drawField(window, m_fenField, fenValidNow || m_fenField.value.empty());
  drawField(window, m_pgnField, pgnValidNow || m_pgnField.value.empty());

  auto palette = ColorPaletteManager::get().palette();

  auto drawButton = [&](const sf::RectangleShape& btn, const sf::Text& txt, bool hovered,
                        bool pressed) {
    drawBevelButton3D(window, btn.getGlobalBounds(), btn.getFillColor(), hovered, pressed);
    sf::Text copy = txt;
    centerText(copy, btn.getGlobalBounds());
    window.draw(copy);
  };

  bool hovCancel = m_cancelBtn.getGlobalBounds().contains(m_lastMousePos);
  bool hovApply = m_applyBtn.getGlobalBounds().contains(m_lastMousePos);
  drawButton(m_cancelBtn, m_cancelText, hovCancel, m_cancelPressed && hovCancel);
  drawButton(m_applyBtn, m_applyText, hovApply, m_applyPressed && hovApply);

  auto drawCaret = [&](const InputField& field) {
    if (!field.active) return;
    float phase = std::fmod(m_caretClock.getElapsedTime().asSeconds(), 1.f);
    if (phase > 0.5f) return;

    float caretHeight = field.multiline ?
                            m_font.getLineSpacing(field.text.getCharacterSize()) * 0.9f :
                            field.box.getSize().y - 12.f;
    sf::Vector2f caretPos = field.text.findCharacterPos(field.value.size());
    float caretX = field.value.empty() ? field.box.getPosition().x + field.padX : caretPos.x;
    float caretTop = field.multiline ? caretPos.y - caretHeight : field.box.getPosition().y +
                                                                 (field.box.getSize().y - caretHeight) * 0.5f;
    caretTop = std::max(caretTop, field.box.getPosition().y + 4.f);
    float maxX = field.box.getPosition().x + field.box.getSize().x - 3.f;
    caretX = std::min(caretX, maxX);
    sf::RectangleShape caret({2.f, caretHeight});
    caret.setFillColor(palette.COL_TEXT);
    caret.setPosition(snapf(caretX), snapf(caretTop));
    window.draw(caret);
  };

  drawCaret(m_fenField);
  drawCaret(m_pgnField);
}

void StartLoadDialog::updateLayout(const sf::Vector2u& windowSize,
                                   const sf::FloatRect& anchorPanel) {
  if (!m_visible) return;
  layout(windowSize, anchorPanel);
  refreshFieldStyles();
}

void StartLoadDialog::applyTheme() {
  auto palette = ColorPaletteManager::get().palette();
  m_overlay.setFillColor(palette.COL_OVERLAY);
  m_panel.setFillColor(palette.COL_PANEL);
  m_panel.setOutlineColor(palette.COL_PANEL_BORDER_ALT);

  m_title.setFillColor(palette.COL_TEXT);
  m_description.setFillColor(palette.COL_MUTED_TEXT);
  m_fenLabel.setFillColor(palette.COL_TEXT);
  m_pgnLabel.setFillColor(palette.COL_TEXT);
  m_hintText.setFillColor(palette.COL_MUTED_TEXT);

  m_cancelBtn.setFillColor(palette.COL_BUTTON);
  m_applyBtn.setFillColor(palette.COL_ACCENT);
  m_cancelText.setFillColor(palette.COL_TEXT);
  m_applyText.setFillColor(constant::COL_DARK_TEXT);

  m_fenField.text.setFillColor(palette.COL_TEXT);
  m_pgnField.text.setFillColor(palette.COL_TEXT);
  m_fenField.placeholder.setFillColor(palette.COL_MUTED_TEXT);
  m_pgnField.placeholder.setFillColor(palette.COL_MUTED_TEXT);

  refreshFieldStyles();
}

void StartLoadDialog::setFen(const std::string& fen) {
  m_fenField.value = fen;
  updateText(m_fenField);
  refreshFieldStyles();
}

void StartLoadDialog::setPgn(const std::string& pgn) {
  m_pgnField.value = pgn;
  updateText(m_pgnField);
  refreshFieldStyles();
}

bool StartLoadDialog::fenValid() const { return basicFenCheck(m_fenField.value); }

bool StartLoadDialog::pgnValid() const { return basicPgnCheck(m_pgnField.value); }

void StartLoadDialog::layout(const sf::Vector2u& windowSize, const sf::FloatRect& anchorPanel) {
  sf::Vector2f panelSize(std::min(560.f, static_cast<float>(windowSize.x) - 80.f),
                         std::min(440.f, static_cast<float>(windowSize.y) - 80.f));
  panelSize.x = std::max(panelSize.x, 360.f);
  panelSize.y = std::max(panelSize.y, 320.f);

  sf::Vector2f anchorCenter(anchorPanel.left + anchorPanel.width * 0.5f,
                            anchorPanel.top + anchorPanel.height * 0.5f);
  sf::Vector2f pos(anchorCenter.x - panelSize.x * 0.5f, anchorCenter.y - panelSize.y * 0.5f);
  pos.x = std::clamp(pos.x, 20.f, static_cast<float>(windowSize.x) - panelSize.x - 20.f);
  pos.y = std::clamp(pos.y, 20.f, static_cast<float>(windowSize.y) - panelSize.y - 20.f);

  m_panel.setSize(panelSize);
  m_panel.setPosition(snap({pos.x, pos.y}));

  m_overlay.setSize({static_cast<float>(windowSize.x), static_cast<float>(windowSize.y)});
  m_overlay.setPosition(0.f, 0.f);

  float left = m_panel.getPosition().x + 28.f;
  float right = m_panel.getPosition().x + panelSize.x - 28.f;
  float width = right - left;
  float y = m_panel.getPosition().y + 30.f;

  m_title.setPosition(snapf(left), snapf(y));
  y += 32.f;

  m_description.setPosition(snapf(left), snapf(y));
  y += 32.f;

  m_fenLabel.setPosition(snapf(left), snapf(y));
  y += 22.f;

  m_fenField.box.setSize({width, 44.f});
  m_fenField.box.setPosition(snap({left, y}));
  y += m_fenField.box.getSize().y + 6.f;

  m_hintText.setPosition(snapf(left), snapf(y));
  y += 26.f;

  m_pgnLabel.setPosition(snapf(left), snapf(y));
  y += 22.f;

  float buttonsHeight = 44.f;
  float buttonsY = m_panel.getPosition().y + panelSize.y - buttonsHeight - 28.f;
  float pgnHeight = buttonsY - y - 18.f;
  if (pgnHeight < 120.f) {
    pgnHeight = 120.f;
    buttonsY = y + pgnHeight + 18.f;
  }

  m_pgnField.box.setSize({width, pgnHeight});
  m_pgnField.box.setPosition(snap({left, y}));

  m_cancelBtn.setSize({150.f, buttonsHeight});
  m_applyBtn.setSize({150.f, buttonsHeight});
  m_cancelBtn.setPosition(snap({left, buttonsY}));
  m_applyBtn.setPosition(snap({right - m_applyBtn.getSize().x, buttonsY}));

  centerText(m_cancelText, m_cancelBtn.getGlobalBounds());
  centerText(m_applyText, m_applyBtn.getGlobalBounds());

  updateText(m_fenField);
  updateText(m_pgnField);
}

void StartLoadDialog::updateText(InputField& field) const {
  field.text.setString(field.value);
  auto bounds = field.text.getLocalBounds();
  if (field.multiline) {
    float x = field.box.getPosition().x + field.padX;
    float y = field.box.getPosition().y + field.padY - bounds.top;
    field.text.setPosition(snapf(x), snapf(y));
  } else {
    float x = field.box.getPosition().x + field.padX;
    float y = field.box.getPosition().y + (field.box.getSize().y - bounds.height) * 0.5f - bounds.top;
    field.text.setPosition(snapf(x), snapf(y));
  }

  auto pb = field.placeholder.getLocalBounds();
  if (field.multiline) {
    float x = field.box.getPosition().x + field.padX;
    float y = field.box.getPosition().y + field.padY - pb.top;
    field.placeholder.setPosition(snapf(x), snapf(y));
  } else {
    float x = field.box.getPosition().x + field.padX;
    float y = field.box.getPosition().y + (field.box.getSize().y - pb.height) * 0.5f - pb.top;
    field.placeholder.setPosition(snapf(x), snapf(y));
  }
}

void StartLoadDialog::drawField(sf::RenderWindow& window, const InputField& field, bool valid) const {
  auto palette = ColorPaletteManager::get().palette();
  sf::RectangleShape box = field.box;
  box.setFillColor(palette.COL_INPUT_BG);
  box.setOutlineThickness(field.active ? 2.f : 1.5f);
  if (field.value.empty()) {
    box.setOutlineColor(field.active ? palette.COL_ACCENT : palette.COL_INPUT_BORDER);
  } else {
    box.setOutlineColor(valid ? palette.COL_VALID : palette.COL_INVALID);
  }
  window.draw(box);

  if (field.value.empty())
    window.draw(field.placeholder);
  else
    window.draw(field.text);
}

void StartLoadDialog::handleTextInput(InputField& field, sf::Uint32 unicode) {
  if (unicode == 8) {  // backspace
    if (!field.value.empty()) {
      field.value.pop_back();
      updateText(field);
      m_caretClock.restart();
    }
    return;
  }

  if (unicode == 13 || unicode == 10) {
    if (field.multiline) {
      field.value.push_back('\n');
      updateText(field);
      m_caretClock.restart();
    }
    return;
  }

  if (unicode < 32 || unicode > 126) return;
  char c = static_cast<char>(unicode);

  if (!field.multiline) {
    float avail = field.box.getSize().x - 2.f * field.padX - 4.f;
    std::string tmp = field.value;
    tmp.push_back(c);
    sf::Text probe(tmp, m_font, field.text.getCharacterSize());
    if (probe.getLocalBounds().width > avail) return;
  }

  field.value.push_back(c);
  updateText(field);
  m_caretClock.restart();
}

void StartLoadDialog::handlePaste(InputField& field) {
  auto clip = sf::Clipboard::getString().toAnsiString();
  if (clip.empty()) return;

  if (!field.multiline) {
    clip.erase(std::remove(clip.begin(), clip.end(), '\n'), clip.end());
    clip.erase(std::remove(clip.begin(), clip.end(), '\r'), clip.end());
  }

  if (!field.multiline) {
    float avail = field.box.getSize().x - 2.f * field.padX - 4.f;
    std::string out = field.value;
    for (char c : clip) {
      if (c < 32 || c > 126) continue;
      sf::Text probe(out + c, m_font, field.text.getCharacterSize());
      if (probe.getLocalBounds().width <= avail)
        out.push_back(c);
      else
        break;
    }
    if (out != field.value) {
      field.value = out;
      updateText(field);
      m_caretClock.restart();
    }
  } else {
    for (char c : clip) {
      if (c == '\r') continue;
      if (c >= 32 || c == '\n' || c == '\t') field.value.push_back(c == '\t' ? ' ' : c);
    }
    updateText(field);
    m_caretClock.restart();
  }
}

void StartLoadDialog::refreshFieldStyles() {
  auto palette = ColorPaletteManager::get().palette();
  m_fenField.placeholder.setFillColor(palette.COL_MUTED_TEXT);
  m_pgnField.placeholder.setFillColor(palette.COL_MUTED_TEXT);
  m_fenField.text.setFillColor(palette.COL_TEXT);
  m_pgnField.text.setFillColor(palette.COL_TEXT);

  // Outline colors handled in drawField; nothing else to cache here.
}

}  // namespace lilia::view

