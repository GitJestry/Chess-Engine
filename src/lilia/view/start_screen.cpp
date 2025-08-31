#include "lilia/view/start_screen.hpp"

#include <SFML/Window/Event.hpp>

#include "lilia/view/render_constants.hpp"

namespace lilia::view {

StartScreen::StartScreen(sf::RenderWindow &window) : m_window(window) {
  m_font.loadFromFile(constant::STR_FILE_PATH_FONT);
  m_logoTex.loadFromFile(constant::STR_FILE_PATH_ICON_LILIA_START_SCREEN);
  m_logo.setTexture(m_logoTex);
  m_logo.setOrigin(m_logo.getLocalBounds().width / 2.f, m_logo.getLocalBounds().height / 2.f);
  m_logo.setPosition(static_cast<float>(m_window.getSize().x) / 2.f,
                     static_cast<float>(m_window.getSize().y) / 2.);
  setupUI();
}

void StartScreen::setupUI() {
  float width = static_cast<float>(m_window.getSize().x);
  float height = static_cast<float>(m_window.getSize().y);

  sf::Vector2f btnSize(180.f, 50.f);
  float leftX = width * 0.2f - btnSize.x / 2.f;
  float rightX = width * 0.8f - btnSize.x / 2.f;
  float baseY = 220.f;

  m_whitePlayerBtn.setSize(btnSize);
  m_whitePlayerBtn.setPosition(leftX, baseY);
  m_whitePlayerBtn.setOutlineColor(sf::Color::White);
  m_whitePlayerBtn.setOutlineThickness(2.f);
  m_whitePlayerText.setFont(m_font);
  m_whitePlayerText.setString("Player");
  m_whitePlayerText.setCharacterSize(20);
  m_whitePlayerText.setFillColor(sf::Color::Black);
  m_whitePlayerText.setPosition(leftX + 20.f, baseY + 12.f);

  m_whiteBotBtn.setSize(btnSize);
  m_whiteBotBtn.setPosition(leftX, baseY + 70.f);
  m_whiteBotBtn.setOutlineColor(sf::Color::White);
  m_whiteBotBtn.setOutlineThickness(2.f);
  m_whiteBotText.setFont(m_font);
  m_whiteBotText.setString("Bot");
  m_whiteBotText.setCharacterSize(20);
  m_whiteBotText.setFillColor(sf::Color::Black);
  m_whiteBotText.setPosition(leftX + 20.f, baseY + 82.f);

  m_whiteLabel.setFont(m_font);
  m_whiteLabel.setString("White");
  m_whiteLabel.setCharacterSize(24);
  m_whiteLabel.setFillColor(sf::Color::White);
  m_whiteLabel.setOrigin(m_whiteLabel.getLocalBounds().width / 2.f,
                         m_whiteLabel.getLocalBounds().height / 2.f);
  m_whiteLabel.setPosition(leftX + btnSize.x / 2.f, baseY - 40.f);

  m_blackPlayerBtn.setSize(btnSize);
  m_blackPlayerBtn.setPosition(rightX, baseY);
  m_blackPlayerBtn.setOutlineColor(sf::Color::White);
  m_blackPlayerBtn.setOutlineThickness(2.f);
  m_blackPlayerText.setFont(m_font);
  m_blackPlayerText.setString("Player");
  m_blackPlayerText.setCharacterSize(20);
  m_blackPlayerText.setFillColor(sf::Color::Black);
  m_blackPlayerText.setPosition(rightX + 20.f, baseY + 12.f);

  m_blackBotBtn.setSize(btnSize);
  m_blackBotBtn.setPosition(rightX, baseY + 70.f);
  m_blackBotBtn.setOutlineColor(sf::Color::White);
  m_blackBotBtn.setOutlineThickness(2.f);
  m_blackBotText.setFont(m_font);
  m_blackBotText.setString("Bot");
  m_blackBotText.setCharacterSize(20);
  m_blackBotText.setFillColor(sf::Color::Black);
  m_blackBotText.setPosition(rightX + 20.f, baseY + 82.f);

  m_blackLabel.setFont(m_font);
  m_blackLabel.setString("Black");
  m_blackLabel.setCharacterSize(24);
  m_blackLabel.setFillColor(sf::Color::White);
  m_blackLabel.setOrigin(m_blackLabel.getLocalBounds().width / 2.f,
                         m_blackLabel.getLocalBounds().height / 2.f);
  m_blackLabel.setPosition(rightX + btnSize.x / 2.f, baseY - 40.f);

  // prepare bot list
  std::vector<std::pair<BotType, PlayerInfo>> bots = {{BotType::Lilia, getBotInfo(BotType::Lilia)}};

  float listYOffset = baseY + 140.f;
  for (std::size_t i = 0; i < bots.size(); ++i) {
    BotOption wOpt;
    wOpt.type = bots[i].first;
    wOpt.box.setSize(btnSize);
    wOpt.box.setPosition(leftX, listYOffset + i * (btnSize.y + 10.f));
    wOpt.box.setFillColor(sf::Color(60, 60, 60));
    wOpt.box.setOutlineColor(sf::Color::White);
    wOpt.box.setOutlineThickness(1.f);
    wOpt.label.setFont(m_font);
    wOpt.label.setString(bots[i].second.name);
    wOpt.label.setCharacterSize(20);
    wOpt.label.setFillColor(sf::Color::White);
    wOpt.label.setPosition(leftX + 10.f, listYOffset + i * (btnSize.y + 10.f) + 10.f);
    m_whiteBotOptions.push_back(wOpt);

    BotOption bOpt = wOpt;
    bOpt.box.setPosition(rightX, listYOffset + i * (btnSize.y + 10.f));
    bOpt.label.setPosition(rightX + 10.f, listYOffset + i * (btnSize.y + 10.f) + 10.f);
    m_blackBotOptions.push_back(bOpt);
  }

  m_startBtn.setSize(sf::Vector2f(200.f, 60.f));
  m_startBtn.setOrigin(100.f, 30.f);
  m_startBtn.setPosition(width / 2.f, height - 60.f);
  m_startBtn.setOutlineColor(sf::Color::White);
  m_startBtn.setOutlineThickness(2.f);
  m_startText.setFont(m_font);
  m_startText.setString("Start");
  m_startText.setCharacterSize(24);
  m_startText.setFillColor(sf::Color::Black);
  m_startText.setOrigin(m_startText.getLocalBounds().width / 2.f,
                        m_startText.getLocalBounds().height / 2.f);
  m_startText.setPosition(m_startBtn.getPosition());

  m_creditText.setFont(m_font);
  m_creditText.setString("Developed By Julian Meyer");
  m_creditText.setCharacterSize(16);
  m_creditText.setFillColor(sf::Color(180, 180, 180));
  sf::FloatRect bounds = m_creditText.getLocalBounds();
  m_creditText.setPosition(width - bounds.width - 10.f, height - bounds.height - 10.f);
}

bool StartScreen::handleMouse(sf::Vector2f pos, StartConfig &cfg) {
  if (m_whitePlayerBtn.getGlobalBounds().contains(pos)) {
    cfg.whiteIsBot = false;
  } else if (m_whiteBotBtn.getGlobalBounds().contains(pos)) {
    cfg.whiteIsBot = true;
  } else if (cfg.whiteIsBot) {
    for (std::size_t i = 0; i < m_whiteBotOptions.size(); ++i) {
      if (m_whiteBotOptions[i].box.getGlobalBounds().contains(pos)) {
        m_whiteBotSelection = i;
        cfg.whiteBot = m_whiteBotOptions[i].type;
      }
    }
  }

  if (m_blackPlayerBtn.getGlobalBounds().contains(pos)) {
    cfg.blackIsBot = false;
  } else if (m_blackBotBtn.getGlobalBounds().contains(pos)) {
    cfg.blackIsBot = true;
  } else if (cfg.blackIsBot) {
    for (std::size_t i = 0; i < m_blackBotOptions.size(); ++i) {
      if (m_blackBotOptions[i].box.getGlobalBounds().contains(pos)) {
        m_blackBotSelection = i;
        cfg.blackBot = m_blackBotOptions[i].type;
      }
    }
  }

  if (m_startBtn.getGlobalBounds().contains(pos)) {
    return true;
  }
  return false;
}

StartConfig StartScreen::run() {
  StartConfig cfg;
  bool startGame = false;
  while (m_window.isOpen() && !startGame) {
    sf::Event event;
    while (m_window.pollEvent(event)) {
      if (event.type == sf::Event::Closed) {
        m_window.close();
      } else if (event.type == sf::Event::MouseButtonPressed &&
                 event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2f pos = m_window.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});
        startGame = handleMouse(pos, cfg);
      }
    }

    sf::Vector2f mouse = m_window.mapPixelToCoords(sf::Mouse::getPosition(m_window));
    sf::Color active(200, 200, 200);
    sf::Color inactive(120, 120, 120);
    sf::Color hoverActive(220, 220, 220);
    sf::Color hoverInactive(140, 140, 140);

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

    for (std::size_t i = 0; i < m_whiteBotOptions.size(); ++i) {
      bool optHover = m_whiteBotOptions[i].box.getGlobalBounds().contains(mouse);
      sf::Color base = (i == m_whiteBotSelection) ? sf::Color(100, 100, 100) : sf::Color(60, 60, 60);
      if (optHover) base = sf::Color(120, 120, 120);
      m_whiteBotOptions[i].box.setFillColor(base);
      m_whiteBotOptions[i].label.setFillColor(
          (cfg.whiteIsBot && i == m_whiteBotSelection) ? sf::Color::Yellow : sf::Color::White);
    }
    for (std::size_t i = 0; i < m_blackBotOptions.size(); ++i) {
      bool optHover = m_blackBotOptions[i].box.getGlobalBounds().contains(mouse);
      sf::Color base = (i == m_blackBotSelection) ? sf::Color(100, 100, 100) : sf::Color(60, 60, 60);
      if (optHover) base = sf::Color(120, 120, 120);
      m_blackBotOptions[i].box.setFillColor(base);
      m_blackBotOptions[i].label.setFillColor(
          (cfg.blackIsBot && i == m_blackBotSelection) ? sf::Color::Yellow : sf::Color::White);
    }

    bool startHover = m_startBtn.getGlobalBounds().contains(mouse);
    m_startBtn.setFillColor(startHover ? hoverActive : active);

    m_window.clear(sf::Color::Black);
    m_window.draw(m_logo);

    m_window.draw(m_whiteLabel);
    m_window.draw(m_whitePlayerBtn);
    m_window.draw(m_whiteBotBtn);
    m_window.draw(m_whitePlayerText);
    m_window.draw(m_whiteBotText);
    if (cfg.whiteIsBot) {
      for (auto &opt : m_whiteBotOptions) {
        m_window.draw(opt.box);
        m_window.draw(opt.label);
      }
    }

    m_window.draw(m_blackLabel);
    m_window.draw(m_blackPlayerBtn);
    m_window.draw(m_blackBotBtn);
    m_window.draw(m_blackPlayerText);
    m_window.draw(m_blackBotText);
    if (cfg.blackIsBot) {
      for (auto &opt : m_blackBotOptions) {
        m_window.draw(opt.box);
        m_window.draw(opt.label);
      }
    }

    m_window.draw(m_startBtn);
    m_window.draw(m_startText);
    m_window.draw(m_creditText);

    m_window.display();
  }

  return cfg;
}

}  // namespace lilia::view
