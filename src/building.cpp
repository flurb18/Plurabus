#include "building.h"

#include <random>
#include <vector>

#include "constants.h"
#include "game.h"

Building::Building(Game *g, BuildingType t, SpawnerID s, int x, int y, int w,
                   int h, int mhp, int updt)
    : game(g), type(t), sid(s), ready(false), hp(1), max_hp(mhp),
      updateCounter(1), updateTime(updt) {

  region = {x, y, w, h};
  center = game->mapUnitAt(x + w / 2, y + h / 2);
}

MapUnit::iterator Building::getIterator() {
  MapUnit *first = game->mapUnitAt(region.x, region.y);
  return first->getIterator(region.w, region.h);
}

bool Building::canUpdate() {
  updateCounter = (updateCounter + 1) % updateTime;
  return (updateCounter == 0 && ready);
}

Tower::Tower(Game *g, SpawnerID s, int x, int y)
    : Building(g, BUILDING_TYPE_TOWER, s, x, y, TOWER_SIZE, TOWER_SIZE,
               MAX_TOWER_HEALTH, TOWER_UPDATE_TIME) {
  for (MapUnit::iterator it = getIterator(); it.hasNext(); it++) {
    it->type = UNIT_TYPE_BUILDING;
    it->building = this;
  }
}

void Tower::update(TowerEvent *tevent) {
  tevent->destroyed = false;
  if (hp > max_hp)
    hp = max_hp;
  if (hp == max_hp)
    ready = true;
  if (canUpdate()) {
    std::vector<AgentID> potentialIDs;
    for (auto it = game->agentDict.begin(); it != game->agentDict.end(); it++) {
      if (it->second->sid != sid) {
        int dx = it->second->unit->x - center->x;
        int dy = it->second->unit->y - center->y;
        if ((dx * dx) + (dy * dy) < TOWER_AOE_RADIUS_SQUARED) {
          potentialIDs.push_back(it->first);
        }
      }
    }
    if (potentialIDs.size() != 0) {
      tevent->destroyed = true;
      int choice = rand() % potentialIDs.size();
      tevent->id = potentialIDs.at(choice);
      tevent->x = center->x;
      tevent->y = center->y;
    }
  }
}

Spawner::Spawner(Game *g, SpawnerID s, int x, int y)
    : Building(g, BUILDING_TYPE_SPAWNER, s, x, y, SPAWNER_SIZE, SPAWNER_SIZE, 1,
               SPAWNER_UPDATE_TIME) {
  for (MapUnit::iterator it = getIterator(); it.hasNext(); it++) {
    it->type = UNIT_TYPE_SPAWNER;
    it->hp = SUBSPAWNER_UNIT_COST;
    it->building = this;
  }
  ready = true;
}

bool Spawner::isDestroyed() {
  for (MapUnit::iterator m = getIterator(); m.hasNext(); m++) {
    if (m->type == UNIT_TYPE_SPAWNER)
      return false;
  }
  return true;
}

void Spawner::update(SpawnerEvent *sevent) {
  sevent->created = false;
  if (canUpdate()) {
    if (sevent->created) {
      sevent->id = game->getNewAgentID();
      sevent->sid = game->getPlayerSpawnID();
    }
  }
}

int Spawner::getNumSpawnUnits() {
  int count = 0;
  for (MapUnit::iterator m = getIterator(); m.hasNext(); m++) {
    if (m->type == UNIT_TYPE_SPAWNER)
      count++;
  }
  return count;
}

bool Spawner::canSpawnAgent(int *retx, int *rety) {
  /* Set spawnX and spawnY to be random unit from the top left corner of the
     spawner to the bottom right */
  int spawnX = (rand() % SPAWNER_SIZE) + region.x;
  int spawnY = (rand() % SPAWNER_SIZE) + region.y;
  /* Then choose a random side of the spawner to spawn on; depending on which
     side we want to spawn on, spawnx or spawnY will either increment or
     decrement by the size of the spawner */
  int whichSide = rand() % 4;
  /* Do the random changing of the spawn location */
  int spawnIncrementOptions[4][2] = {{SPAWNER_SIZE, 0},
                                     {0, SPAWNER_SIZE},
                                     {-SPAWNER_SIZE, 0},
                                     {0, -SPAWNER_SIZE}};
  spawnX += spawnIncrementOptions[whichSide][0];
  spawnY += spawnIncrementOptions[whichSide][1];
  if (spawnX < 0 || spawnX >= game->getSize() || spawnY < 0 ||
      spawnY >= game->getSize())
    return false;
  MapUnit *uptr = game->mapUnitAt(spawnX, spawnY);
  if (uptr->type == UNIT_TYPE_EMPTY && !uptr->isMarked()) {
    *retx = spawnX;
    *rety = spawnY;
    uptr->mark();
    return true;
  } else {
    return false;
  }
}

Subspawner::Subspawner(Game *g, SpawnerID s, int x, int y)
    : Building(g, BUILDING_TYPE_SUBSPAWNER, s, x, y, SUBSPAWNER_SIZE,
               SUBSPAWNER_SIZE, 1, SUBSPAWNER_UPDATE_TIME) {
  for (MapUnit::iterator it = getIterator(); it.hasNext(); it++) {
    it->building = this;
  }
}

bool Subspawner::isDestroyed() {
  for (MapUnit::iterator m = getIterator(); m.hasNext(); m++) {
    if (m->type == UNIT_TYPE_SPAWNER)
      return false;
  }
  return true;
}

void Subspawner::update(SpawnerEvent *sevent) {
  sevent->created = false;
  if (!ready) {
    ready = true;
    for (MapUnit::iterator m = getIterator(); m.hasNext(); m++) {
      if (m->type != UNIT_TYPE_SPAWNER || m->hp < SUBSPAWNER_UNIT_COST)
        ready = false;
    }
  }
  if (canUpdate()) {
    sevent->created = canSpawnAgent(&sevent->x, &sevent->y);
    if (sevent->created) {
      sevent->id = game->getNewAgentID();
      sevent->sid = game->getPlayerSpawnID();
    }
  }
}

bool Subspawner::canSpawnAgent(int *retx, int *rety) {
  /* Set spawnX and spawnY to be random unit from the top left corner of the
     spawner to the bottom right */
  int spawnX = (rand() % SUBSPAWNER_SIZE) + region.x;
  int spawnY = (rand() % SUBSPAWNER_SIZE) + region.y;
  /* Then choose a random side of the spawner to spawn on; depending on which
     side we want to spawn on, spawnx or spawnY will either increment or
     decrement by the size of the spawner */
  int whichSide = rand() % 4;
  /* Do the random changing of the spawn location */
  int spawnIncrementOptions[4][2] = {{SUBSPAWNER_SIZE, 0},
                                     {0, SUBSPAWNER_SIZE},
                                     {-SUBSPAWNER_SIZE, 0},
                                     {0, -SUBSPAWNER_SIZE}};
  spawnX += spawnIncrementOptions[whichSide][0];
  spawnY += spawnIncrementOptions[whichSide][1];
  if (spawnX < 0 || spawnX >= game->getSize() || spawnY < 0 ||
      spawnY >= game->getSize())
    return false;
  MapUnit *uptr = game->mapUnitAt(spawnX, spawnY);
  if (uptr->type == UNIT_TYPE_EMPTY && !uptr->isMarked()) {
    *retx = spawnX;
    *rety = spawnY;
    uptr->mark();
    return true;
  } else {
    return false;
  }
}

Bomb::Bomb(Game *g, SpawnerID s, int x, int y)
    : Building(g, BUILDING_TYPE_BOMB, s, x, y, BOMB_SIZE, BOMB_SIZE,
               MAX_BOMB_HEALTH, 1) {

  for (MapUnit::iterator it = getIterator(); it.hasNext(); it++) {
    it->type = UNIT_TYPE_BUILDING;
    it->building = this;
  }
}

void Bomb::update(BombEvent *bevent) {
  bevent->detonated = false;
  if (hp > max_hp)
    hp = max_hp;
  if (hp == max_hp)
    ready = true;
  if (ready) {
    bevent->x = center->x;
    bevent->y = center->y;
    bevent->detonated = true;
  }
}
