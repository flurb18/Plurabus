#include <cstdlib>
#include <ctime>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <string>

#include "constants.h"
#include "display.h"
#include "game.h"

void mainloop(void *arg) {
  Game *g = (Game *)arg;
  g->mainLoop();
}

int mainloop_thread(void *arg) {
  Game *g = (Game *)arg;
  while (g->getContext() != GAME_CONTEXT_EXIT) {
    g->mainLoop();
  }
  return 0;
}

int handleAppEvents(void *arg, SDL_Event *event) {
  Game *g = (Game *)arg;
  switch (event->type) {
  case SDL_APP_DIDENTERBACKGROUND:
    g->end(DONE_STATUS_BACKGROUND);
    break;
  case SDL_APP_TERMINATING:
    delete g;
    break;
  default:
    return 1;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  /* Set the random number generator seed */
  srand(time(0));
  int gameMode = atoi(argv[5]);
  int gamePanelSize = atoi(argv[1]);
  double gameInitScale = atof(argv[2]);
  bool mobile = (argc > 6);
  Game *g = new Game(gameMode, GAME_SIZE, gamePanelSize, gameInitScale, argv[3],
                     argv[4], mobile);

  SDL_SetEventFilter(handleAppEvents, (void *)g);

#ifdef __EMSCRIPTEN__

  emscripten_set_main_loop_arg(mainloop, (void *)g, 0, 1);

#else

  mainloop_thread((void *)g);

#endif

  delete g;
  return 0;
}
