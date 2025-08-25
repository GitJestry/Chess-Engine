#include "lilia/uci/UCI.hpp"

#ifdef LILIA_UI
#include "lilia/app/App.hpp"
#endif

int main() {
#ifdef LILIA_UI
  // start App
  lilia::app::App app;
  return app.run();
#elif defined(LILIA_ENGINE)
  // Engine-only
  lilia::UCI uci;
  return uci.run();
#else
#endif
}
