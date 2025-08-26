#include "lilia/app/app.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
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
  if (s.empty()) return true;  // default
  std::string t = toLower(trim(s));
  if (t == "n" || t == "no" || t == "nah" || t == "0" || t == "false") return false;
  return true;
}

int App::parseIntInRange(const std::string& s, int defaultVal, int minVal, int maxVal) {
  std::string t = trim(s);
  if (t.empty()) return defaultVal;
  if (!std::all_of(t.begin(), t.end(), [](unsigned char c) { return std::isdigit(c); }))
    return -1;
  int val = std::stoi(t);
  if (val < minVal || val > maxVal) return -1;
  return val;
}

void App::promptStartOptions() {
  std::cout << "Player color (white / black) [Standard: white]: ";
  std::string colorInput;
  std::getline(std::cin, colorInput);
  std::string colorNorm = toLower(trim(colorInput));
  if (!colorNorm.empty()) {
    if (colorNorm == "black" || colorNorm == "b")
      m_playerColor = core::Color::Black;
    else
      m_playerColor = core::Color::White;  // anything else -> white
  } else {
    m_playerColor = core::Color::White;
  }

  std::cout << "Enemy is bot? (yes / no) [Standard: yes]: ";
  std::string botInput;
  std::getline(std::cin, botInput);
  m_vsBot = parseYesNoDefaultTrue(botInput);

  std::cout << "Startposition as FEN [empty = Standard-Start]: ";
  std::string fenInput;
  std::getline(std::cin, fenInput);
  std::string fenTrim = trim(fenInput);
  if (fenTrim.empty()) {
    m_startFen = core::START_FEN;
  } else {
    m_startFen = fenInput;  // use exactly what the user typed (trimmed)
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
  // read options from stdin
  promptStartOptions();

  // model / engine init
  engine::Engine::init();
  lilia::view::TextureTable::getInstance().preLoad();

  // create window
  sf::RenderWindow window(
      sf::VideoMode(lilia::view::constant::WINDOW_PX_SIZE, lilia::view::constant::WINDOW_PX_SIZE),
      "Lilia", sf::Style::Titlebar | sf::Style::Close);

  // create game objects (local scope to ensure correct lifetimes)
  {
    lilia::model::ChessGame chessgame;
    lilia::view::GameView view(window);
    lilia::controller::GameController gController(view, chessgame);

    // start the game using GameController wrapper that delegates to GameManager
    gController.startGame(m_playerColor, m_startFen, m_vsBot, m_thinkTimeMs,
                          m_searchDepth);

    // main loop
    sf::Clock clock;
    while (window.isOpen()) {
      float dt = clock.restart().asSeconds();
      sf::Event event;
      while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) window.close();
        gController.handleEvent(event);
      }
      gController.update(dt);
      window.clear(sf::Color::Blue);
      gController.render();
      window.display();
    }
  }

  return 0;
}

}  // namespace lilia::app
