#include "objective.h"

#include "constants.h"
#include "game.h"
#include "mapunit.h"

Objective::Objective(ObjectiveType t, int s, Game *g, SDL_Rect r,
                     SpawnerID _sid)
    : type(t), strength(s), done(false), sid(_sid), game(g), region(r) {
  citer = nullptr;
  switch (type) {
  case OBJECTIVE_TYPE_BUILD_WALL:
    citer = new concentric_iterator(g, region.x, region.y, region.w, region.h);
    break;
  case OBJECTIVE_TYPE_BUILD_TOWER:
    started = false;
    break;
  case OBJECTIVE_TYPE_BUILD_SUBSPAWNER:
    citer = new concentric_iterator(g, region.x, region.y, region.w, region.h);
    started = false;
    break;
  case OBJECTIVE_TYPE_BUILD_BOMB:
    started = false;
    break;
  default:
    started = true;
    break;
  }
}

Objective::Objective(Objective *sup, ObjectiveType t, int s, Game *g,
                     SDL_Rect r, SpawnerID _sid)
    : Objective(t, s, g, r, _sid) {
  super = sup;
}

MapUnit::iterator Objective::getIterator() {
  MapUnit *first = game->mapUnitAt(region.x, region.y);
  return first->getIterator(region.w, region.h);
}

bool Objective::isDone() {
  switch (type) {
  case OBJECTIVE_TYPE_BUILD_SUBSPAWNER:
  case OBJECTIVE_TYPE_BUILD_WALL:
  case OBJECTIVE_TYPE_BUILD_DOOR:
  case OBJECTIVE_TYPE_BUILD_TOWER:
  case OBJECTIVE_TYPE_BUILD_BOMB:
    return done;
  default:
    return false;
  }
}

bool Objective::regionIsReadyForBuilding() {
  for (MapUnit::iterator it = getIterator(); it.hasNext(); it++) {
    if (!((it->type == UNIT_TYPE_EMPTY) ||
          (it->type == UNIT_TYPE_AGENT &&
           it->agent->sid == game->playerSpawnID)))
      return false;
  }
  return true;
}

void Objective::updateCiter(UnitType desired, int desiredHP) {
  SpawnerID psid = game->getPlayerSpawnID();
  bool past_done = true;
  for (MapUnit *m : citer->past) {
    if (m->type != desired) {
      past_done = false;
    }
  }
  if (!past_done) {
    done = true;
    return;
  }
  bool current_done = true;
  for (MapUnit *m : citer->current) {
    if (m->type == desired) {
      if (m->hp < desiredHP) {
        current_done = false;
        m->playerDict[psid].objective = this;
        m->setEmptyNeighborScents(strength);
      }
    } else {
      current_done = false;
      if (m->type == UNIT_TYPE_EMPTY) {
        m->playerDict[psid].objective = this;
        m->setScent(strength);
      }
    }
  }
  if (current_done) {
    if (citer->hasNext()) {
      (*citer)++;
    } else {
      done = true;
    }
  }
}

void Objective::update() {
  SpawnerID psid = game->getPlayerSpawnID();
  switch (type) {
  case OBJECTIVE_TYPE_BUILD_WALL:
    updateCiter(UNIT_TYPE_WALL, 0);
    break;
  case OBJECTIVE_TYPE_BUILD_SUBSPAWNER:
    updateCiter(UNIT_TYPE_SPAWNER, SUBSPAWNER_UNIT_COST);
    break;
  case OBJECTIVE_TYPE_ATTACK:
    done = true;
    for (MapUnit::iterator m = getIterator(); m.hasNext(); m++) {
      switch (m->type) {
      case UNIT_TYPE_AGENT:
        if (m->agent->getSpawnID() != game->getPlayerSpawnID()) {
          done = false;
          m->playerDict[psid].objective = this;
          m->setEmptyNeighborScents(strength);
        }
        break;
      case UNIT_TYPE_SPAWNER:
        if (m->building->type == BUILDING_TYPE_SPAWNER &&
            m->building->sid == game->getPlayerSpawnID())
          break;
        done = false;
        m->playerDict[psid].objective = this;
        m->setEmptyNeighborScents(strength);
        break;
      case UNIT_TYPE_DOOR:
        if (m->door->sid != game->getPlayerSpawnID()) {
          done = false;
          m->playerDict[psid].objective = this;
          m->setEmptyNeighborScents(strength);
        }
        break;
      case UNIT_TYPE_WALL:
        done = false;
        m->playerDict[psid].objective = this;
        m->setEmptyNeighborScents(strength);
        break;
      case UNIT_TYPE_BUILDING:
        done = false;
        m->playerDict[psid].objective = this;
        m->setEmptyNeighborScents(strength);
        break;
      default:
        break;
      }
    }
    break;
  case OBJECTIVE_TYPE_GOTO:
    for (MapUnit::iterator m = getIterator(); m.hasNext(); m++) {
      if (m->type == UNIT_TYPE_EMPTY)
        m->setScent(strength);
    }
  case OBJECTIVE_TYPE_BUILD_DOOR:
    done = true;
    for (MapUnit::iterator m = getIterator(); m.hasNext(); m++) {
      switch (m->type) {
      case UNIT_TYPE_DOOR:
        if (m->door->hp < MAX_DOOR_HEALTH) {
          done = false;
          m->playerDict[psid].objective = this;
          m->setEmptyNeighborScents(strength);
        }
        break;
      case UNIT_TYPE_WALL:
        done = false;
        m->playerDict[psid].objective = this;
        m->setEmptyNeighborScents(strength);
        break;
      default:
        break;
      }
    }
    break;
  case OBJECTIVE_TYPE_BUILD_TOWER:
    done = true;
    if (!started) {
      MapUnit *center =
          game->mapUnitAt(region.x + region.w / 2, region.y + region.h / 2);
      if (center->type == UNIT_TYPE_EMPTY) {
        center->setScent(strength);
        center->playerDict[psid].objective = this;
      }
      done = false;
      break;
    }
    for (MapUnit::iterator m = getIterator(); m.hasNext(); m++) {
      if (m->building->hp < m->building->max_hp) {
        m->playerDict[psid].objective = this;
        m->setEmptyNeighborScents(strength);
        done = false;
      }
    }
    break;
  case OBJECTIVE_TYPE_BUILD_BOMB:
    done = true;
    if (!started) {
      MapUnit *center =
          game->mapUnitAt(region.x + region.w / 2, region.y + region.h / 2);
      if (center->type == UNIT_TYPE_EMPTY) {
        center->setScent(strength);
        center->playerDict[psid].objective = this;
      }
      done = false;
      break;
    }
    for (MapUnit::iterator m = getIterator(); m.hasNext(); m++) {
      if (m->building->hp < m->building->max_hp) {
        m->playerDict[psid].objective = this;
        m->setEmptyNeighborScents(strength);
        done = false;
      }
    }
    break;
  default:
    break;
  }
}

Objective::~Objective() {

  switch (type) {
  case OBJECTIVE_TYPE_BUILD_WALL:
  case OBJECTIVE_TYPE_BUILD_SUBSPAWNER:
    delete citer;
  default:
    break;
  }
}
