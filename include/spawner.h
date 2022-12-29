#ifndef SPAWNER_H
#define SPAWNER_H

#include <vector>

#include "event.h"

/* Forward declarations */
class Agent;
class Game;
struct MapUnit;

class Spawner {
  friend class Game;
private:
  /* How many ticks between Agent spawn attempts */
  unsigned int timeToCreateAgent;
  MapUnit* topLeft;
  int size;
  SpawnerID sid;
  bool canSpawnAgent(int*, int*);
public:
  Game* game;
  Spawner(Game*, MapUnit*, SpawnerID, unsigned int, unsigned int);
  ~Spawner();
  void update(SpawnerEvent*);
  bool isDestroyed();
  SpawnerID getSpawnID();
};

#endif
