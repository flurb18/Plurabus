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

void *mainloop_thread(void *arg) {
  Game *g = (Game*)arg;
  while (g->getContext() != GAME_CONTEXT_EXIT) {
    g->mainLoop();
  }
  delete g;
  return NULL;
}

int main(int argc, char* argv[]) {
  /* Set the random number generator seed */
  srand(time(0));
  int gameSize = atoi(argv[1]);
  int gamePanelSize = atoi(argv[2]);
  double gameInitScale = atof(argv[3]);
  bool mobile = (argc > 5);
  Game *g = new Game(
		     gameSize,
		     gamePanelSize,
		     gameInitScale,
		     argv[4],
		     mobile
		     );

#ifdef __EMSCRIPTEN__

  emscripten_set_main_loop_arg(mainloop, (void*)g, 0, 1);

#else

  pthread_t mainThread;
  pthread_create(&mainThread, NULL, &mainloop_thread, (void*)g);
  pthread_join(mainThread, NULL);

#endif
}
