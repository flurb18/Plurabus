#include <cstdlib>
#include <ctime>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <string>

#include "display.h"
#include "game.h"

void mainloop(void *arg) {
  Game *g = (Game*)arg;
  g->mainLoop();
}

int main(int argc, char* argv[]) {
  /* Set the random number generator seed */
  srand(time(0));
  int gameSize = atoi(argv[1]);
  int gamePanelSize = atoi(argv[2]);
  double gameInitScale = atof(argv[3]);
  Game *g = new Game(
		     gameSize,
		     gamePanelSize,
		     gameInitScale,
		     argv[4],
		     argv[5]
		     );

#ifdef __EMSCRIPTEN__

  emscripten_set_main_loop_arg(mainloop, (void*)g, 0, 1);

#else
  
  while (g->getContext() != GAME_CONTEXT_EXIT) {
    mainloop((void*)g);
  }

#endif

  delete g;
  return 0;
}
