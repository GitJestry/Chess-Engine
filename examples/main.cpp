#include "lilia/app/app.hpp"
#include "lilia/uci/uci.hpp"

int main() {
#ifdef LILIA_UI
  lilia::app::App app;
  return app.run();
#else
  // starte UCI-Engine
  lilia::UCI uci;
  return uci.run();
  // hier UCI-Loop wie vorher
#endif
}
