#ifndef OBJECTIVE_H
#define OBJECTIVE_H

#include <SDL2/SDL_rect.h>
#include <deque>

#include "mapunit.h"

enum ObjectiveType {
  OBJECTIVE_TYPE_BUILD_WALL,
  OBJECTIVE_TYPE_BUILD_DOOR,
  OBJECTIVE_TYPE_BUILD_TOWER,
  OBJECTIVE_TYPE_BUILD_SUBSPAWNER,
  OBJECTIVE_TYPE_BUILD_BOMB,
  OBJECTIVE_TYPE_GOTO,
  OBJECTIVE_TYPE_ATTACK
};

class Game;

struct Objective {
  ObjectiveType type;
  int strength;
  int numUnitsDone, numUnitsRequired;
  bool started;
  bool done;
  SpawnerID sid;
  concentric_iterator *citer;
  Game* game;
  SDL_Rect region;
  Objective(ObjectiveType, int, Game*, SDL_Rect, SpawnerID);
  MapUnit::iterator getIterator();
  bool isDone();
  bool regionIsReadyForBuilding();
  void updateCiter(UnitType, int);
  void update();
  ~Objective();
};

#endif
