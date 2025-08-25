#include "lilia/uci/uci.hpp"

namespace lilia {

int UCI::run() {
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line == "uci") {
      std::cout << "id name " << m_name << "\n";
      std::cout << "id version " << m_version << "\n";
      showOptions();
      std::cout << "uciok\n";
    } else if (line == "isready") {
      std::cout << "readyok\n";
    } else if (line.substr(0, 8) == "position") {
      if (line.find("startpos") != std::string::npos) {
        game.setPosition(core::START_FEN);
      }
      auto movesPos = line.find("moves");
      if (movesPos != std::string::npos) {
        std::string movesStr = line.substr(movesPos + 6);
        std::istringstream iss(movesStr);
        std::string moveUci;
        while (iss >> moveUci) {
          game.doMoveUCI(moveUci);
        }
      }
    } else if (line.substr(0, 2) == "go") {
      std::atomic<bool> cancelToken(false);
      auto futureMove = lilia.requestMove(game, cancelToken);
      model::Move best = futureMove.get();
      std::cout << "bestmove " << lilia::controller::move_to_uci(best) << "\n";
    } else if (line == "quit") {
      break;
    } else if (line.substr(0, 9) == "setoption") {
      setOption();
    }
  }
  return 0;
}
void UCI::setOption() {
  // TODO
}
void UCI::showOptions() {
  // TODO
}
}  // namespace lilia
