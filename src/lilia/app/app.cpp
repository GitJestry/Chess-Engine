#include "lilia/app/app.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include <algorithm>
#include <cctype>
#include <iostream>

#include "lilia/controller/game_controller.hpp"
#include "lilia/engine/engine.hpp"
#include "lilia/model/chess_game.hpp"
#include "lilia/view/game_view.hpp"
#include "lilia/view/texture_table.hpp"

namespace lilia::app {

std::string App::trim(const std::string& s) {
  size_t start = 0;
  while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
  size_t end = s.size();
  while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
  return s.substr(start, end - start);
}

std::string App::toLower(const std::string& s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return out;
}

bool App::parseYesNoDefaultTrue(const std::string& s) {
  if (s.empty()) return true;
  std::string normalized = toLower(trim(s));
  if (normalized == "n" || normalized == "no" || normalized == "nah" || normalized == "0" ||
      normalized == "false")
    return false;
  return true;
}

int App::parseIntInRange(const std::string& s, int defaultVal, int minVal, int maxVal) {
  std::string t = trim(s);
  if (t.empty()) return defaultVal;
  if (!std::all_of(t.begin(), t.end(), [](unsigned char c) { return std::isdigit(c); })) return -1;
  int val = std::stoi(t);
  if (val < minVal || val > maxVal) return -1;
  return val;
}

void App::promptStartOptions() {
  std::cout << "Player color (white / black) [Standard: white]: ";
  std::string playerColorInput;
  std::getline(std::cin, playerColorInput);
  std::string normalizedColor = toLower(trim(playerColorInput));
  m_player_color = (normalizedColor == "black" || normalizedColor == "b") ? core::Color::Black
                                                                          : core::Color::White;

  std::cout << "Enemy is bot? (yes / no) [Standard: yes]: ";
  std::string botInput;
  std::getline(std::cin, botInput);
  m_vs_bot = parseYesNoDefaultTrue(botInput);

  std::cout << "Startposition as FEN [empty = Standard-Start]: ";
  std::string fenInput;
  std::getline(std::cin, fenInput);
  std::string fenTrim = trim(fenInput);
  if (fenTrim.empty()) {
    m_start_fen = core::START_FEN;
  } else {
    m_start_fen = fenInput;  // use exactly what the user typed (trimmed)
  }

  // Think time in seconds
  while (true) {
    std::cout << "Bot think time in seconds [Standard: 1]: ";
    std::string thinkInput;
    std::getline(std::cin, thinkInput);
    int val = parseIntInRange(thinkInput, 1, 1, 60);
    if (val != -1) {
      m_thinkTimeMs = val * 1000;  // convert to ms
      break;
    }
    std::cout << "Please enter a number between 1 and 60.\n";
  }

  // Search depth
  while (true) {
    std::cout << "Bot search depth [Standard: 5]: ";
    std::string depthInput;
    std::getline(std::cin, depthInput);
    int val = parseIntInRange(depthInput, 5, 1, 20);
    if (val != -1) {
      m_searchDepth = val;
      break;
    }
    std::cout << "Please enter a number between 1 and 20.\n";
  }
}

int App::run() {
  promptStartOptions();

  engine::Engine::init();
  lilia::view::TextureTable::getInstance().preLoad();

  sf::RenderWindow window(
      sf::VideoMode(lilia::view::constant::WINDOW_PX_SIZE, lilia::view::constant::WINDOW_PX_SIZE),
      "Lilia", sf::Style::Titlebar | sf::Style::Close);

  {
    lilia::model::ChessGame chessGame;
    lilia::view::GameView gameView(window);
    lilia::controller::GameController gameController(gameView, chessGame);

    // start the game using GameController wrapper that delegates to GameManager
    gameController.startGame(m_player_color, m_start_fen, m_vs_bot, m_thinkTimeMs, m_searchDepth);

    sf::Clock clock;
    while (window.isOpen()) {
      float deltaSeconds = clock.restart().asSeconds();
      sf::Event event;
      while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) window.close();
        gameController.handleEvent(event);
      }
      gameController.update(deltaSeconds);
      window.clear(sf::Color::Blue);
      gameController.render();
      window.display();
    }
  }

  return 0;
}

}  // namespace lilia::app
