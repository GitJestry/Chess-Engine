#include "lilia/view/start_screen.hpp"

#include <SFML/Window/Event.hpp>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "lilia/model/chess_game.hpp"
#include "lilia/view/render_constants.hpp"

namespace lilia::view {

// Helper: center origin of an sf::Text to its bounds
static void centerOrigin(sf::Text& t) {
  sf::FloatRect r = t.getLocalBounds();
  t.setOrigin(r.left + r.width / 2.f, r.top + r.height / 2.f);
}

// Helper: draw a soft shadow under a rectangle, then the rectangle itself
// (Keeps outlines on the original rect; shadow is just a blurred-looking offset quad)
static void drawWithShadow(sf::RenderWindow& window, const sf::RectangleShape& r) {
  sf::RectangleShape shadow = r;
  shadow.move(0.f, 3.f);
  shadow.setFillColor(sf::Color(0, 0, 0, 120));
  shadow.setOutlineThickness(0.f);
  window.draw(shadow);
  window.draw(r);
}

StartScreen::StartScreen(sf::RenderWindow& window) : m_window(window) {
  // Load assets
  m_font.loadFromFile(constant::STR_FILE_PATH_FONT);
  m_logoTex.loadFromFile(constant::STR_FILE_PATH_ICON_LILIA_START_SCREEN);

  // Logo
  m_logo.setTexture(m_logoTex);
  m_logo.setOrigin(m_logo.getLocalBounds().width / 2.f, m_logo.getLocalBounds().height / 2.f);
  m_logo.setPosition(static_cast<float>(m_window.getSize().x) / 2.f,
                     static_cast<float>(m_window.getSize().y) / 2.f);

  // Ensure lists start closed
  m_showWhiteBotList = false;
  m_showBlackBotList = false;
  m_whiteListForceHide = false;
  m_blackListForceHide = false;

  setupUI();
}

void StartScreen::setupUI() {
  const float width = static_cast<float>(m_window.getSize().x);
  const float height = static_cast<float>(m_window.getSize().y);

  // Responsive layout anchors
  const float colLeftX = width * 0.18f;
  const float colRightX = width * 0.82f;
  const float colWidth = 200.f;
  const float btnH = 52.f;
  const float btnW = std::min(colWidth, width * 0.24f);
  const float colYBase = std::max(210.f, height * 0.30f);

  const sf::Vector2f btnSize(btnW, btnH);

  // Palette
  const sf::Color outline(255, 255, 255);
  const float outlineThick = 2.f;

  // White column
  m_whitePlayerBtn.setSize(btnSize);
  m_whitePlayerBtn.setPosition(colLeftX - btnW / 2.f, colYBase);
  m_whitePlayerBtn.setOutlineColor(outline);
  m_whitePlayerBtn.setOutlineThickness(outlineThick);

  m_whiteBotBtn.setSize(btnSize);
  m_whiteBotBtn.setPosition(colLeftX - btnW / 2.f, colYBase + btnH + 18.f);
  m_whiteBotBtn.setOutlineColor(outline);
  m_whiteBotBtn.setOutlineThickness(outlineThick);

  m_whitePlayerText.setFont(m_font);
  m_whitePlayerText.setString("Player");
  m_whitePlayerText.setCharacterSize(22);
  m_whitePlayerText.setFillColor(sf::Color::Black);
  m_whitePlayerText.setPosition(m_whitePlayerBtn.getPosition().x + 18.f,
                                m_whitePlayerBtn.getPosition().y + 12.f);

  m_whiteBotText.setFont(m_font);
  m_whiteBotText.setCharacterSize(22);
  m_whiteBotText.setFillColor(sf::Color::Black);
  m_whiteBotText.setPosition(m_whiteBotBtn.getPosition().x + 18.f,
                             m_whiteBotBtn.getPosition().y + 12.f);

  m_whiteLabel.setFont(m_font);
  m_whiteLabel.setString("White");
  m_whiteLabel.setCharacterSize(34);
  m_whiteLabel.setStyle(sf::Text::Bold);
  m_whiteLabel.setFillColor(sf::Color::White);
  centerOrigin(m_whiteLabel);
  m_whiteLabel.setPosition(colLeftX, m_whitePlayerBtn.getPosition().y - 58.f);

  // Black column
  m_blackPlayerBtn.setSize(btnSize);
  m_blackPlayerBtn.setPosition(colRightX - btnW / 2.f, colYBase);
  m_blackPlayerBtn.setOutlineColor(outline);
  m_blackPlayerBtn.setOutlineThickness(outlineThick);

  m_blackBotBtn.setSize(btnSize);
  m_blackBotBtn.setPosition(colRightX - btnW / 2.f, colYBase + btnH + 18.f);
  m_blackBotBtn.setOutlineColor(outline);
  m_blackBotBtn.setOutlineThickness(outlineThick);

  m_blackPlayerText.setFont(m_font);
  m_blackPlayerText.setString("Player");
  m_blackPlayerText.setCharacterSize(22);
  m_blackPlayerText.setFillColor(sf::Color::Black);
  m_blackPlayerText.setPosition(m_blackPlayerBtn.getPosition().x + 18.f,
                                m_blackPlayerBtn.getPosition().y + 12.f);

  m_blackBotText.setFont(m_font);
  m_blackBotText.setCharacterSize(22);
  m_blackBotText.setFillColor(sf::Color::Black);
  m_blackBotText.setPosition(m_blackBotBtn.getPosition().x + 18.f,
                             m_blackBotBtn.getPosition().y + 12.f);

  m_blackLabel.setFont(m_font);
  m_blackLabel.setString("Black");
  m_blackLabel.setCharacterSize(34);
  m_blackLabel.setStyle(sf::Text::Bold);
  m_blackLabel.setFillColor(sf::Color::White);
  centerOrigin(m_blackLabel);
  m_blackLabel.setPosition(colRightX, m_blackPlayerBtn.getPosition().y - 58.f);

  // Prepare bot list (anchor is computed per-frame to avoid hover gaps)
  std::vector<std::pair<BotType, PlayerInfo>> bots = {
      {BotType::Lilia, getBotInfo(BotType::Lilia)}
      // Add more here later
  };

  // Default label: "Bot (Name)"
  const std::string defaultBotLabel = "Bot (" + bots.front().second.name + ")";
  m_whiteBotText.setString(defaultBotLabel);
  m_blackBotText.setString(defaultBotLabel);

  m_whiteBotOptions.clear();
  m_blackBotOptions.clear();
  m_whiteBotSelection = 0;
  m_blackBotSelection = 0;

  for (std::size_t i = 0; i < bots.size(); ++i) {
    BotOption wOpt;
    wOpt.type = bots[i].first;
    wOpt.box.setSize(btnSize);  // final position set each frame
    wOpt.box.setFillColor(sf::Color(80, 80, 80));
    wOpt.box.setOutlineColor(outline);
    wOpt.box.setOutlineThickness(outlineThick);
    wOpt.label.setFont(m_font);
    wOpt.label.setString(bots[i].second.name);
    wOpt.label.setCharacterSize(22);
    wOpt.label.setFillColor(sf::Color::White);
    m_whiteBotOptions.push_back(wOpt);

    BotOption bOpt = wOpt;
    m_blackBotOptions.push_back(bOpt);
  }

  // Start button
  m_startBtn.setSize(sf::Vector2f(220.f, 64.f));
  m_startBtn.setOrigin(110.f, 32.f);
  m_startBtn.setPosition(width / 2.f, height - 72.f);
  m_startBtn.setOutlineColor(outline);
  m_startBtn.setOutlineThickness(outlineThick);

  m_startText.setFont(m_font);
  m_startText.setString("Start");
  m_startText.setCharacterSize(26);
  m_startText.setFillColor(sf::Color::Black);
  centerOrigin(m_startText);
  m_startText.setPosition(m_startBtn.getPosition());

  // Credit
  m_creditText.setFont(m_font);
  m_creditText.setString("@Developed By Julian Meyer");
  m_creditText.setCharacterSize(16);
  m_creditText.setFillColor(sf::Color(180, 180, 180));
  sf::FloatRect c = m_creditText.getLocalBounds();
  m_creditText.setPosition(width - c.width - 10.f, height - c.height - 10.f);

  // FEN popup
  m_fenPopup.setSize(sf::Vector2f(width * 0.8f, height * 0.4f));
  m_fenPopup.setPosition(width * 0.1f, height * 0.3f);
  m_fenPopup.setFillColor(sf::Color(60, 60, 60, 240));
  m_fenPopup.setOutlineColor(outline);
  m_fenPopup.setOutlineThickness(outlineThick);

  m_fenInputBox.setSize(sf::Vector2f(m_fenPopup.getSize().x - 40.f, 50.f));
  m_fenInputBox.setPosition(m_fenPopup.getPosition().x + 20.f,
                             m_fenPopup.getPosition().y + 40.f);
  m_fenInputBox.setFillColor(sf::Color::White);
  m_fenInputBox.setOutlineColor(outline);
  m_fenInputBox.setOutlineThickness(outlineThick);

  m_fenInputText.setFont(m_font);
  m_fenInputText.setCharacterSize(24);
  m_fenInputText.setFillColor(sf::Color::Black);
  m_fenInputText.setPosition(m_fenInputBox.getPosition().x + 5.f,
                             m_fenInputBox.getPosition().y + 10.f);

  const float fenBtnW = (m_fenPopup.getSize().x - 60.f) / 2.f;
  const float fenBtnY = m_fenPopup.getPosition().y + m_fenPopup.getSize().y - 60.f;
  m_fenBackBtn.setSize(sf::Vector2f(fenBtnW, 40.f));
  m_fenBackBtn.setPosition(m_fenPopup.getPosition().x + 20.f, fenBtnY);
  m_fenBackBtn.setOutlineColor(outline);
  m_fenBackBtn.setOutlineThickness(outlineThick);

  m_fenContinueBtn.setSize(sf::Vector2f(fenBtnW, 40.f));
  m_fenContinueBtn.setPosition(m_fenBackBtn.getPosition().x + fenBtnW + 20.f, fenBtnY);
  m_fenContinueBtn.setOutlineColor(outline);
  m_fenContinueBtn.setOutlineThickness(outlineThick);

  m_fenBackText.setFont(m_font);
  m_fenBackText.setString("Back");
  m_fenBackText.setCharacterSize(22);
  m_fenBackText.setFillColor(sf::Color::Black);
  centerOrigin(m_fenBackText);
  m_fenBackText.setPosition(m_fenBackBtn.getPosition().x + fenBtnW / 2.f,
                            m_fenBackBtn.getPosition().y + 20.f);

  m_fenContinueText.setFont(m_font);
  m_fenContinueText.setString("Continue");
  m_fenContinueText.setCharacterSize(22);
  m_fenContinueText.setFillColor(sf::Color::Black);
  centerOrigin(m_fenContinueText);
  m_fenContinueText.setPosition(m_fenContinueBtn.getPosition().x + fenBtnW / 2.f,
                                m_fenContinueBtn.getPosition().y + 20.f);

  m_fenErrorText.setFont(m_font);
  m_fenErrorText.setCharacterSize(20);
  m_fenErrorText.setFillColor(sf::Color::Red);
  centerOrigin(m_fenErrorText);
  m_fenErrorText.setPosition(width / 2.f,
                             m_fenInputBox.getPosition().y + m_fenInputBox.getSize().y + 30.f);
}

bool StartScreen::handleMouse(sf::Vector2f pos, StartConfig& cfg) {
  if (m_showFenPopup) return handleFenMouse(pos, cfg);

  // WHITE column interactions
  if (m_whitePlayerBtn.getGlobalBounds().contains(pos)) {
    cfg.whiteIsBot = false;
  } else if (m_whiteBotBtn.getGlobalBounds().contains(pos)) {
    // Clicking the bot button while the list is force-hidden (but still hovering) re-opens it.
    if (m_whiteListForceHide && !m_showWhiteBotList) {
      m_whiteListForceHide = false;
      m_showWhiteBotList = true;
    }
    // Confirm "white is bot" without collapsing the list
    cfg.whiteIsBot = true;
    if (!m_whiteBotOptions.empty()) cfg.whiteBot = m_whiteBotOptions[m_whiteBotSelection].type;
  } else if (m_showWhiteBotList) {
    for (std::size_t i = 0; i < m_whiteBotOptions.size(); ++i) {
      if (m_whiteBotOptions[i].box.getGlobalBounds().contains(pos)) {
        m_whiteBotSelection = i;
        cfg.whiteIsBot = true;
        cfg.whiteBot = m_whiteBotOptions[i].type;
        m_whiteBotText.setString(m_whiteBotOptions[i].label.getString());
        m_whiteBotText.setPosition(m_whiteBotBtn.getPosition().x + 18.f,
                                   m_whiteBotBtn.getPosition().y + 12.f);
        // Close list after selection; keep it closed until hover fully leaves the zone.
        m_showWhiteBotList = false;
        m_whiteListForceHide = true;
      }
    }
  }

  // BLACK column interactions
  if (m_blackPlayerBtn.getGlobalBounds().contains(pos)) {
    cfg.blackIsBot = false;
  } else if (m_blackBotBtn.getGlobalBounds().contains(pos)) {
    if (m_blackListForceHide && !m_showBlackBotList) {
      m_blackListForceHide = false;
      m_showBlackBotList = true;
    }
    cfg.blackIsBot = true;
    if (!m_blackBotOptions.empty()) cfg.blackBot = m_blackBotOptions[m_blackBotSelection].type;
  } else if (m_showBlackBotList) {
    for (std::size_t i = 0; i < m_blackBotOptions.size(); ++i) {
      if (m_blackBotOptions[i].box.getGlobalBounds().contains(pos)) {
        m_blackBotSelection = i;
        cfg.blackIsBot = true;
        cfg.blackBot = m_blackBotOptions[i].type;
        m_blackBotText.setString(m_blackBotOptions[i].label.getString());
        m_blackBotText.setPosition(m_blackBotBtn.getPosition().x + 18.f,
                                   m_blackBotBtn.getPosition().y + 12.f);
        m_showBlackBotList = false;
        m_blackListForceHide = true;
      }
    }
  }

  // Start
  if (m_startBtn.getGlobalBounds().contains(pos)) {
    m_showFenPopup = true;
  }
  return false;
}

bool StartScreen::handleFenMouse(sf::Vector2f pos, StartConfig& cfg) {
  if (m_fenBackBtn.getGlobalBounds().contains(pos)) {
    m_showFenPopup = false;
    m_fenString.clear();
    m_fenInputText.setString("");
    m_fenInputBox.setOutlineColor(sf::Color::White);
    m_fenInputBox.setFillColor(sf::Color::White);
    m_showError = false;
  } else if (m_fenContinueBtn.getGlobalBounds().contains(pos)) {
    std::string fen = m_fenString.empty() ? core::START_FEN : m_fenString;
    if (isValidFen(fen)) {
      cfg.fen = fen;
      m_fenInputBox.setFillColor(sf::Color::White);
      return true;
    } else {
      m_fenInputBox.setOutlineColor(sf::Color::Red);
      m_fenInputBox.setFillColor(sf::Color(255, 200, 200));
      m_showError = true;
      m_errorClock.restart();
    }
  }
  return false;
}

bool StartScreen::isValidFen(const std::string& fen) {
  try {
    model::ChessGame g;
    g.setPosition(fen);
  } catch (...) {
    return false;
  }
  return true;
}

StartConfig StartScreen::run() {
  StartConfig cfg;
  bool startGame = false;

  // Colors (slightly brighter, clearer states)
  const sf::Color active(210, 210, 210);
  const sf::Color inactive(110, 110, 110);
  const sf::Color hoverActive(235, 235, 235);
  const sf::Color hoverInactive(140, 140, 140);

  // Lambda: re-anchor dropdown lists immediately under their bot buttons every frame.
  auto positionLists = [&]() {
    const float spacing = 6.f;  // gap between button and first option
    const float vpad = 4.f;     // gap between options
    const float labelX = 10.f;
    const float labelY = 10.f;

    // WHITE list anchored to WHITE bot button
    const float y0W = m_whiteBotBtn.getPosition().y + m_whiteBotBtn.getSize().y + spacing;
    for (std::size_t i = 0; i < m_whiteBotOptions.size(); ++i) {
      auto& opt = m_whiteBotOptions[i];
      opt.box.setSize(m_whiteBotBtn.getSize());
      opt.box.setPosition(m_whiteBotBtn.getPosition().x,
                          y0W + static_cast<float>(i) * (m_whiteBotBtn.getSize().y + vpad));
      opt.label.setPosition(opt.box.getPosition().x + labelX, opt.box.getPosition().y + labelY);
    }

    // BLACK list anchored to BLACK bot button
    const float y0B = m_blackBotBtn.getPosition().y + m_blackBotBtn.getSize().y + spacing;
    for (std::size_t i = 0; i < m_blackBotOptions.size(); ++i) {
      auto& opt = m_blackBotOptions[i];
      opt.box.setSize(m_blackBotBtn.getSize());
      opt.box.setPosition(m_blackBotBtn.getPosition().x,
                          y0B + static_cast<float>(i) * (m_blackBotBtn.getSize().y + vpad));
      opt.label.setPosition(opt.box.getPosition().x + labelX, opt.box.getPosition().y + labelY);
    }
  };

  while (m_window.isOpen() && !startGame) {
    // Events
    sf::Event event;
    while (m_window.pollEvent(event)) {
      if (event.type == sf::Event::Closed) {
        m_window.close();
      } else if (m_showFenPopup) {
        if (event.type == sf::Event::MouseButtonPressed &&
            event.mouseButton.button == sf::Mouse::Left) {
          sf::Vector2f pos =
              m_window.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});
          startGame = handleMouse(pos, cfg);
        } else if (event.type == sf::Event::TextEntered) {
          if (event.text.unicode == 8) {
            if (!m_fenString.empty()) m_fenString.pop_back();
          } else if (event.text.unicode >= 32 && event.text.unicode < 127) {
            m_fenString += static_cast<char>(event.text.unicode);
          }
          m_fenInputText.setString(m_fenString);
          m_fenInputBox.setOutlineColor(sf::Color::White);
          m_fenInputBox.setFillColor(sf::Color::White);
          m_showError = false;
        } else if (event.type == sf::Event::KeyPressed) {
          if (event.key.code == sf::Keyboard::Enter) {
            std::string fen = m_fenString.empty() ? core::START_FEN : m_fenString;
            if (isValidFen(fen)) {
              cfg.fen = fen;
              startGame = true;
            } else {
              m_fenInputBox.setOutlineColor(sf::Color::Red);
              m_fenInputBox.setFillColor(sf::Color(255, 200, 200));
              m_showError = true;
              m_errorClock.restart();
            }
          } else if (event.key.code == sf::Keyboard::Escape) {
            m_showFenPopup = false;
            m_fenString.clear();
            m_fenInputText.setString("");
            m_fenInputBox.setOutlineColor(sf::Color::White);
            m_fenInputBox.setFillColor(sf::Color::White);
            m_showError = false;
          }
        }
      } else if (event.type == sf::Event::MouseButtonPressed &&
                 event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2f pos = m_window.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});
        startGame = handleMouse(pos, cfg);
      } else if (event.type == sf::Event::KeyPressed &&
                 event.key.code == sf::Keyboard::Escape) {
        // Optional: allow closing lists with Esc
        m_showWhiteBotList = false;
        m_showBlackBotList = false;
        m_whiteListForceHide = false;
        m_blackListForceHide = false;
      }
    }

    // Keep dropdowns attached to their buttons; this removes hover "gaps"
    positionLists();

    // Hover logic
    sf::Vector2f mouse = m_window.mapPixelToCoords(sf::Mouse::getPosition(m_window));

    if (!m_showFenPopup) {
      // Button fills depend on whether that side is bot or player, and hover state
      bool hover = m_whitePlayerBtn.getGlobalBounds().contains(mouse);
      m_whitePlayerBtn.setFillColor(cfg.whiteIsBot ? (hover ? hoverInactive : inactive)
                                                   : (hover ? hoverActive : active));

      hover = m_whiteBotBtn.getGlobalBounds().contains(mouse);
      m_whiteBotBtn.setFillColor(cfg.whiteIsBot ? (hover ? hoverActive : active)
                                                : (hover ? hoverInactive : inactive));

      hover = m_blackPlayerBtn.getGlobalBounds().contains(mouse);
      m_blackPlayerBtn.setFillColor(cfg.blackIsBot ? (hover ? hoverInactive : inactive)
                                                   : (hover ? hoverActive : active));

      hover = m_blackBotBtn.getGlobalBounds().contains(mouse);
      m_blackBotBtn.setFillColor(cfg.blackIsBot ? (hover ? hoverActive : active)
                                                : (hover ? hoverInactive : inactive));

      // Compute hover for dropdown zones
      bool whiteBotHover = m_whiteBotBtn.getGlobalBounds().contains(mouse);
      bool whiteListHover = false;
      for (auto& opt : m_whiteBotOptions) {
        if (opt.box.getGlobalBounds().contains(mouse)) {
          whiteListHover = true;
          break;
        }
      }
      if (!whiteBotHover && !whiteListHover) m_whiteListForceHide = false;
      m_showWhiteBotList = !m_whiteListForceHide && (whiteBotHover || whiteListHover);

      bool blackBotHover = m_blackBotBtn.getGlobalBounds().contains(mouse);
      bool blackListHover = false;
      for (auto& opt : m_blackBotOptions) {
        if (opt.box.getGlobalBounds().contains(mouse)) {
          blackListHover = true;
          break;
        }
      }
      if (!blackBotHover && !blackListHover) m_blackListForceHide = false;
      m_showBlackBotList = !m_blackListForceHide && (blackBotHover || blackListHover);

      // Highlight selected option and hover in lists
      for (std::size_t i = 0; i < m_whiteBotOptions.size(); ++i) {
        bool optHover = m_whiteBotOptions[i].box.getGlobalBounds().contains(mouse);
        sf::Color base =
            (i == m_whiteBotSelection) ? sf::Color(100, 100, 100) : sf::Color(80, 80, 80);
        if (optHover) base = sf::Color(120, 120, 120);
        m_whiteBotOptions[i].box.setFillColor(base);
        m_whiteBotOptions[i].label.setFillColor(
            (cfg.whiteIsBot && i == m_whiteBotSelection) ? sf::Color::Yellow : sf::Color::White);
      }
      for (std::size_t i = 0; i < m_blackBotOptions.size(); ++i) {
        bool optHover = m_blackBotOptions[i].box.getGlobalBounds().contains(mouse);
        sf::Color base =
            (i == m_blackBotSelection) ? sf::Color(100, 100, 100) : sf::Color(80, 80, 80);
        if (optHover) base = sf::Color(120, 120, 120);
        m_blackBotOptions[i].box.setFillColor(base);
        m_blackBotOptions[i].label.setFillColor(
            (cfg.blackIsBot && i == m_blackBotSelection) ? sf::Color::Yellow : sf::Color::White);
      }

      // Start button hover
      bool startHover = m_startBtn.getGlobalBounds().contains(mouse);
      m_startBtn.setFillColor(startHover ? hoverActive : active);
    } else {
      bool backHover = m_fenBackBtn.getGlobalBounds().contains(mouse);
      bool contHover = m_fenContinueBtn.getGlobalBounds().contains(mouse);
      m_fenBackBtn.setFillColor(backHover ? hoverActive : active);
      m_fenContinueBtn.setFillColor(contHover ? hoverActive : active);

      if (m_showError) {
        float sec = m_errorClock.getElapsedTime().asSeconds();
        if (sec > 2.f) {
          m_showError = false;
        } else {
          sf::Uint8 a = static_cast<sf::Uint8>(255.f * (1.f - sec / 2.f));
          m_fenErrorText.setFillColor(sf::Color(255, 0, 0, a));
        }
      }
    }

    // ---- DRAW ----
    m_window.clear(sf::Color::Black);

    // Logo
    m_window.draw(m_logo);

    // White side
    m_window.draw(m_whiteLabel);
    drawWithShadow(m_window, m_whitePlayerBtn);
    drawWithShadow(m_window, m_whiteBotBtn);
    m_window.draw(m_whitePlayerText);
    m_window.draw(m_whiteBotText);
    if (m_showWhiteBotList) {
      for (auto& opt : m_whiteBotOptions) {
        drawWithShadow(m_window, opt.box);
        m_window.draw(opt.label);
      }
    }

    // Black side
    m_window.draw(m_blackLabel);
    drawWithShadow(m_window, m_blackPlayerBtn);
    drawWithShadow(m_window, m_blackBotBtn);
    m_window.draw(m_blackPlayerText);
    m_window.draw(m_blackBotText);
    if (m_showBlackBotList) {
      for (auto& opt : m_blackBotOptions) {
        drawWithShadow(m_window, opt.box);
        m_window.draw(opt.label);
      }
    }

    // Start + credit
    drawWithShadow(m_window, m_startBtn);
    m_window.draw(m_startText);
    m_window.draw(m_creditText);

    if (m_showFenPopup) {
      sf::RectangleShape overlay(sf::Vector2f(static_cast<float>(m_window.getSize().x),
                                              static_cast<float>(m_window.getSize().y)));
      overlay.setFillColor(sf::Color(0, 0, 0, 150));
      m_window.draw(overlay);
      drawWithShadow(m_window, m_fenPopup);
      drawWithShadow(m_window, m_fenInputBox);
      m_window.draw(m_fenInputText);
      drawWithShadow(m_window, m_fenBackBtn);
      drawWithShadow(m_window, m_fenContinueBtn);
      m_window.draw(m_fenBackText);
      m_window.draw(m_fenContinueText);
      if (m_showError) m_window.draw(m_fenErrorText);
    }

    m_window.display();
  }

  return cfg;
}

}  // namespace lilia::view
