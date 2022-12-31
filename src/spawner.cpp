#include "spawner.h"

#include <random>

#include "agent.h"
#include "game.h"

/* Constructor of the spawner also tells the argument map unit that it has a spawner,
   and sets the map units where the spawner is */
Spawner::Spawner(Game* g, MapUnit* u, SpawnerID spwnid,	\
                 unsigned int s, unsigned int t_) : timeToCreateAgent(t_), topLeft(u), size(s), sid(spwnid), \
                  game(g) {
  for (MapUnit::iterator iter = topLeft->getIterator(size, size); iter.hasNext(); iter++) {
    iter->type = UNIT_TYPE_SPAWNER;
    iter->spawner = this;
    iter->hp = SUBSPAWNER_UNIT_COST;
  }
}

/* Update the spawner through one tick of game time */
void Spawner::update(SpawnerEvent *sevent) {
  int x, y;
  if (canSpawnAgent(&x, &y)) {
    sevent->x = x;
    sevent->y = y;
    sevent->created = true;
    sevent->id = game->getNewAgentID();
    sevent->sid = sid;
  } else {
    sevent->created = false;
  }
}

bool Spawner::isDestroyed() {
  for (MapUnit::iterator iter = topLeft->getIterator(size, size); iter.hasNext(); iter++) {
    if (iter->type == UNIT_TYPE_SPAWNER) return false;
  }
  return true;
}

/* Try to spawn an agent. Spawner spawns in areas of its size up, down left and
   right; so if all spawn map units are filled, it looks like a cross with the
   spawner at the center and agents filling boxes of the spawner's size on all
   sides */
bool Spawner::canSpawnAgent(int *retx, int *rety) {
  /* Set spawnX and spawnY to be random unit from the top left corner of the
     spawner to the bottom right */
  int spawnX = (rand() % size) + topLeft->x;
  int spawnY = (rand() % size) + topLeft->y;
  /* Then choose a random side of the spawner to spawn on; depending on which
     side we want to spawn on, spawnx or spawnY will either increment or
     decrement by the size of the spawner */
  int whichSide = rand() % 4;
  /* Convert unsigned int size to normal int so decrement works */
  int s = size;
  /* Do the random changing of the spawn location */
  int spawnIncrementOptions[4][2] = {{s, 0}, {0, s}, {-s, 0}, {0, -s}};
  spawnX += spawnIncrementOptions[whichSide][0];
  spawnY += spawnIncrementOptions[whichSide][1];
  MapUnit *uptr = game->mapUnitAt(spawnX,spawnY);
  if (uptr->type == UNIT_TYPE_EMPTY && !uptr->isMarked()) {
    *retx = spawnX;
    *rety = spawnY;
    uptr->mark();
    return true;
  } else {
    return false;
  }
}

SpawnerID Spawner::getSpawnID() {
  return sid;
}

Spawner::~Spawner() {
  
}
