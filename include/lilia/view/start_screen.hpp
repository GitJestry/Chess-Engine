#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

#include "lilia/bot/bot_info.hpp"
#include "lilia/constants.hpp"

namespace lilia::view {

struct StartConfig {
  bool whiteIsBot{false};
  BotType whiteBot{BotType::Lilia};
  bool blackIsBot{true};
  BotType blackBot{BotType::Lilia};
  std::string fen{core::START_FEN};

};

struct BotOption {
  BotType type;
  sf::RectangleShape box;
  sf::Text label;
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
  sf::Text m_devByText;    // "@Developed by Julian Meyer" bottom-right
  sf::Text m_fenInfoText;  // subtle hint below FEN box

  sf::RectangleShape m_whitePlayerBtn;
  sf::RectangleShape m_whiteBotBtn;
  sf::Text m_whitePlayerText;
  sf::Text m_whiteBotText;
  sf::Text m_whiteLabel;
  std::vector<BotOption> m_whiteBotOptions;
  std::size_t m_whiteBotSelection{0};
  bool m_showWhiteBotList{false};
  bool m_whiteListForceHide{false};

  sf::RectangleShape m_blackPlayerBtn;
  sf::RectangleShape m_blackBotBtn;
  sf::Text m_blackPlayerText;
  sf::Text m_blackBotText;
  sf::Text m_blackLabel;
  std::vector<BotOption> m_blackBotOptions;
  std::size_t m_blackBotSelection{0};
  bool m_showBlackBotList{false};
  bool m_blackListForceHide{false};

  sf::RectangleShape m_startBtn;
  sf::Text m_startText;
  sf::Text m_creditText;

  // FEN popup UI
  bool m_showFenPopup{false};
  sf::RectangleShape m_fenPopup;
  sf::RectangleShape m_fenInputBox;
  sf::Text m_fenInputText;
  sf::RectangleShape m_fenBackBtn;
  sf::RectangleShape m_fenContinueBtn;
  sf::Text m_fenBackText;
  sf::Text m_fenContinueText;
  sf::Text m_fenErrorText;
  std::string m_fenString;
  sf::Clock m_errorClock;
  bool m_showError{false};

  void setupUI();
  bool handleMouse(sf::Vector2f pos, StartConfig &cfg);
  bool handleFenMouse(sf::Vector2f pos, StartConfig &cfg);
  bool isValidFen(const std::string &fen);
};

}  // namespace lilia::view
