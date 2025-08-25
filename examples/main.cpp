#include "lilia/uci/UCI.hpp"

#ifdef LILIA_UI
#include "lilia/app/App.hpp"
#endif

int main() {
#ifdef LILIA_UI
  // UI-Build: starte die App
  lilia::app::App app;
  return app.run();
#elif defined(LILIA_ENGINE)
  // Engine-only: starte UCI-Loop
  lilia::UCI uci;
  return uci.run();
#else
#endif
}
