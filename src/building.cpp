#include "building.h"

#include <vector>
#include <random>

#include "constants.h"
#include "game.h"

Building::Building(Game *g, BuildingType t, SpawnerID s, int x, int y, int w, int h, int mhp, int updt):
  game(g),
  type(t),
  sid(s),
  ready(false),
  hp(1),
  max_hp(mhp),
  updateCounter(1),
  updateTime(updt) {

  region = { x, y, w, h };
  for (MapUnit::iterator it = getIterator(); it.hasNext(); it++) {
    it->type = UNIT_TYPE_BUILDING;
    it->building = this;
  }
}

MapUnit::iterator Building::getIterator() {
  MapUnit *first = game->mapUnitAt(region.x, region.y);
  return first->getIterator(region.w, region.h);
}

bool Building::canUpdate() {
  updateCounter = (updateCounter + 1) % updateTime;
  return (updateCounter == 0 && ready);
}

Tower::Tower(Game *g, SpawnerID s, TowerID t, int x, int y):
  Building(g, BUILDING_TYPE_TOWER, s, x, y, TOWER_SIZE, TOWER_SIZE, TOWER_MAX_HP, TOWER_UPDATE_TIME),
  tid(t) {}

void Tower::update(TowerEvent *tevent) {
  tevent->destroyed = false;
  if (canUpdate()) {
    std::vector<AgentID> potentialIDs;
    for (auto it = game->agentDict.begin(); it != game->agentDict.end(); it++) {
      if (it->second->sid != sid) {
	int dx = it->second->unit->x - (region.x + (region.w/2));
	int dy = it->second->unit->y - (region.y + (region.h/2));
	if ((dx*dx) + (dy*dy) < TOWER_AOE_RADIUS_SQUARED) {
	  potentialIDs.push_back(it->first);
	}
      }
    }
    if (potentialIDs.size() != 0) {
      tevent->destroyed = true;
      int choice = rand() % potentialIDs.size();
      tevent->id = potentialIDs.at(choice);
      tevent->tid = tid;
    }
  }
}
