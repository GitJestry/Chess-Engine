#pragma once

#include <SFML/Graphics.hpp>
#include <vector>

#include "lilia/bot/bot_info.hpp"

namespace lilia::view {

struct StartConfig {
  bool whiteIsBot{false};
  BotType whiteBot{BotType::Lilia};
  bool blackIsBot{true};
  BotType blackBot{BotType::Lilia};
};

class StartScreen {
 public:
  explicit StartScreen(sf::RenderWindow &window);
  StartConfig run();

 private:
  sf::RenderWindow &m_window;
  sf::Font m_font;
  sf::Texture m_logoTex;
  sf::Sprite m_logo;
  sf::Text m_title;

  sf::RectangleShape m_whitePlayerBtn;
  sf::RectangleShape m_whiteBotBtn;
  sf::Text m_whitePlayerText;
  sf::Text m_whiteBotText;
  std::vector<std::pair<BotType, sf::Text>> m_whiteBotOptions;
  std::size_t m_whiteBotSelection{0};

  sf::RectangleShape m_blackPlayerBtn;
  sf::RectangleShape m_blackBotBtn;
  sf::Text m_blackPlayerText;
  sf::Text m_blackBotText;
  std::vector<std::pair<BotType, sf::Text>> m_blackBotOptions;
  std::size_t m_blackBotSelection{0};

  sf::RectangleShape m_startBtn;
  sf::Text m_startText;

  void setupUI();
  bool handleMouse(sf::Vector2f pos, StartConfig &cfg);
};

}  // namespace lilia::view

