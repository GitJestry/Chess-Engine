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
  float leftX = width * 0.25f - btnSize.x / 2.f;
  float rightX = width * 0.75f - btnSize.x / 2.f;
  float baseY = 220.f;

  m_whitePlayerBtn.setSize(btnSize);
  m_whitePlayerBtn.setPosition(leftX, baseY);
  m_whitePlayerText.setFont(m_font);
  m_whitePlayerText.setString("Player");
  m_whitePlayerText.setCharacterSize(20);
  m_whitePlayerText.setFillColor(sf::Color::Black);
  m_whitePlayerText.setPosition(leftX + 20.f, baseY + 12.f);

  m_whiteBotBtn.setSize(btnSize);
  m_whiteBotBtn.setPosition(leftX, baseY + 70.f);
  m_whiteBotText.setFont(m_font);
  m_whiteBotText.setString("Bot");
  m_whiteBotText.setCharacterSize(20);
  m_whiteBotText.setFillColor(sf::Color::Black);
  m_whiteBotText.setPosition(leftX + 20.f, baseY + 82.f);

  m_blackPlayerBtn.setSize(btnSize);
  m_blackPlayerBtn.setPosition(rightX, baseY);
  m_blackPlayerText.setFont(m_font);
  m_blackPlayerText.setString("Player");
  m_blackPlayerText.setCharacterSize(20);
  m_blackPlayerText.setFillColor(sf::Color::Black);
  m_blackPlayerText.setPosition(rightX + 20.f, baseY + 12.f);

  m_blackBotBtn.setSize(btnSize);
  m_blackBotBtn.setPosition(rightX, baseY + 70.f);
  m_blackBotText.setFont(m_font);
  m_blackBotText.setString("Bot");
  m_blackBotText.setCharacterSize(20);
  m_blackBotText.setFillColor(sf::Color::Black);
  m_blackBotText.setPosition(rightX + 20.f, baseY + 82.f);

  // prepare bot list
  std::vector<std::pair<BotType, PlayerInfo>> bots = {{BotType::Lilia, getBotInfo(BotType::Lilia)}};

  float listYOffset = baseY + 140.f;
  for (std::size_t i = 0; i < bots.size(); ++i) {
    sf::Text t;
    t.setFont(m_font);
    t.setString(bots[i].second.name);
    t.setCharacterSize(20);
    t.setFillColor(sf::Color::White);
    t.setPosition(leftX, listYOffset + i * 30.f);
    m_whiteBotOptions.push_back({bots[i].first, t});

    sf::Text t2 = t;
    t2.setPosition(rightX, listYOffset + i * 30.f);
    m_blackBotOptions.push_back({bots[i].first, t2});
  }

  m_startBtn.setSize(sf::Vector2f(200.f, 60.f));
  m_startBtn.setOrigin(100.f, 30.f);
  m_startBtn.setPosition(width / 2.f, height - 100.f);
  m_startText.setFont(m_font);
  m_startText.setString("Start");
  m_startText.setCharacterSize(24);
  m_startText.setFillColor(sf::Color::Black);
  m_startText.setOrigin(m_startText.getLocalBounds().width / 2.f,
                        m_startText.getLocalBounds().height / 2.f);
  m_startText.setPosition(m_startBtn.getPosition());
}

bool StartScreen::handleMouse(sf::Vector2f pos, StartConfig &cfg) {
  if (m_whitePlayerBtn.getGlobalBounds().contains(pos)) {
    cfg.whiteIsBot = false;
  } else if (m_whiteBotBtn.getGlobalBounds().contains(pos)) {
    cfg.whiteIsBot = true;
  } else if (cfg.whiteIsBot) {
    for (std::size_t i = 0; i < m_whiteBotOptions.size(); ++i) {
      if (m_whiteBotOptions[i].second.getGlobalBounds().contains(pos)) {
        m_whiteBotSelection = i;
        cfg.whiteBot = m_whiteBotOptions[i].first;
      }
    }
  }

  if (m_blackPlayerBtn.getGlobalBounds().contains(pos)) {
    cfg.blackIsBot = false;
  } else if (m_blackBotBtn.getGlobalBounds().contains(pos)) {
    cfg.blackIsBot = true;
  } else if (cfg.blackIsBot) {
    for (std::size_t i = 0; i < m_blackBotOptions.size(); ++i) {
      if (m_blackBotOptions[i].second.getGlobalBounds().contains(pos)) {
        m_blackBotSelection = i;
        cfg.blackBot = m_blackBotOptions[i].first;
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

    m_whitePlayerBtn.setFillColor(cfg.whiteIsBot ? sf::Color(100, 100, 100)
                                                 : sf::Color(200, 200, 200));
    m_whiteBotBtn.setFillColor(cfg.whiteIsBot ? sf::Color(200, 200, 200)
                                              : sf::Color(100, 100, 100));
    m_blackPlayerBtn.setFillColor(cfg.blackIsBot ? sf::Color(100, 100, 100)
                                                 : sf::Color(200, 200, 200));
    m_blackBotBtn.setFillColor(cfg.blackIsBot ? sf::Color(200, 200, 200)
                                              : sf::Color(100, 100, 100));

    for (std::size_t i = 0; i < m_whiteBotOptions.size(); ++i) {
      m_whiteBotOptions[i].second.setFillColor(
          (cfg.whiteIsBot && i == m_whiteBotSelection) ? sf::Color::Yellow : sf::Color::White);
    }
    for (std::size_t i = 0; i < m_blackBotOptions.size(); ++i) {
      m_blackBotOptions[i].second.setFillColor(
          (cfg.blackIsBot && i == m_blackBotSelection) ? sf::Color::Yellow : sf::Color::White);
    }

    m_startBtn.setFillColor(sf::Color(200, 200, 200));

    m_window.clear(sf::Color::Black);
    m_window.draw(m_logo);

    m_window.draw(m_whitePlayerBtn);
    m_window.draw(m_whiteBotBtn);
    m_window.draw(m_whitePlayerText);
    m_window.draw(m_whiteBotText);
    if (cfg.whiteIsBot) {
      for (auto &t : m_whiteBotOptions) m_window.draw(t.second);
    }

    m_window.draw(m_blackPlayerBtn);
    m_window.draw(m_blackBotBtn);
    m_window.draw(m_blackPlayerText);
    m_window.draw(m_blackBotText);
    if (cfg.blackIsBot) {
      for (auto &t : m_blackBotOptions) m_window.draw(t.second);
    }

    m_window.draw(m_startBtn);
    m_window.draw(m_startText);

    m_window.display();
  }

  return cfg;
}

}  // namespace lilia::view
