#ifndef BUILDING_H
#define BUILDING_H

#include <SDL2/SDL_rect.h>

#include "event.h"
#include "mapunit.h"

typedef enum BuildingType {
  BUILDING_TYPE_TOWER,
  BUILDING_TYPE_SPAWNER,
  BUILDING_TYPE_SUBSPAWNER,
  BUILDING_TYPE_BOMB
} BuildingType;

class Game;
class Agent;
struct Objective;

class Building {
  friend class Game;
  friend class Agent;
  friend struct Objective;
protected:
  Game *game;
  BuildingType type;
  SDL_Rect region;
  SpawnerID sid;
  bool ready;
  int hp;
  int max_hp;
  int updateCounter;
  int updateTime;
  MapUnit *center;
public:
  MapUnit::iterator getIterator();
  bool canUpdate();
  Building(Game*, BuildingType, SpawnerID, int, int, int, int, int, int);
};

class Tower: public Building {
  friend class Game;
public:
  Tower(Game*, SpawnerID, int, int);
  void update(TowerEvent*);
};

class Spawner: public Building {
  friend class Game;
private:
  bool isDestroyed();
  bool canSpawnAgent(int*, int*);
public:
  Spawner(Game*, SpawnerID, int, int);
  void update(SpawnerEvent*);
};

class Subspawner: public Building {
  friend class Game;
private:
  bool isDestroyed();
  bool canSpawnAgent(int*, int*);
public:
  Subspawner(Game*, SpawnerID, int, int);
  void update(SpawnerEvent*);
};

class Bomb: public Building {
  friend class Game;
private:
public:
  Bomb(Game*, SpawnerID, int, int);
  void update(BombEvent*);
};

#endif
