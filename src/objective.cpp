#include "objective.h"

#include "constants.h"
#include "game.h"
#include "mapunit.h"

Objective::Objective(ObjectiveType t, int s, Game *g, SDL_Rect r,
                     SpawnerID _sid)
    : type(t), strength(s), done(false), sid(_sid), game(g), region(r) {
  switch (type) {
  case OBJECTIVE_TYPE_BUILD_WALL:
    for (int i = 0; region.w - (2 * i) > 0 && region.h - (2 * i) > 0; i++) {
      SDL_Rect sub = {region.x + i, region.y + i, region.w - (2 * i),
                      region.h - (2 * i)};
      subObjectives.push_front(new Objective(
          this, OBJECTIVE_TYPE_BUILD_WALL_SUB, strength, game, sub, sid));
    }
    iter = subObjectives.begin();
    break;
  case OBJECTIVE_TYPE_BUILD_TOWER:
    started = false;
    break;
  case OBJECTIVE_TYPE_BUILD_SUBSPAWNER:
    started = false;
    for (int i = 0; region.w - (2 * i) > 0 && region.h - (2 * i) > 0; i++) {
      SDL_Rect sub = {region.x + i, region.y + i, region.w - (2 * i),
                      region.h - (2 * i)};
      subObjectives.push_front(new Objective(
          this, OBJECTIVE_TYPE_BUILD_SUBSPAWNER_SUB, strength, game, sub, sid));
    }
    iter = subObjectives.begin();
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
  case OBJECTIVE_TYPE_BUILD_WALL:
  case OBJECTIVE_TYPE_BUILD_SUBSPAWNER:
    return (iter == subObjectives.end());
  case OBJECTIVE_TYPE_BUILD_WALL_SUB:
  case OBJECTIVE_TYPE_BUILD_DOOR:
  case OBJECTIVE_TYPE_BUILD_TOWER:
  case OBJECTIVE_TYPE_BUILD_SUBSPAWNER_SUB:
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

void Objective::update() {
  SpawnerID psid = game->getPlayerSpawnID();
  switch (type) {
  case OBJECTIVE_TYPE_BUILD_WALL:
    if (iter != subObjectives.end()) {
      (*iter)->update();
      if ((*iter)->isDone()) {
        iter++;
      }
    }
    break;
  case OBJECTIVE_TYPE_BUILD_WALL_SUB:
    done = true;
    for (MapUnit::iterator m = getIterator(); m.hasNext(); m++) {
      if (m->type != UNIT_TYPE_WALL && m->type != UNIT_TYPE_DOOR) {
        done = false;
        if (m->type == UNIT_TYPE_EMPTY) {
          m->playerDict[psid].objective = this;
          m->setScent(strength);
        }
      }
    }
    break;
  case OBJECTIVE_TYPE_BUILD_SUBSPAWNER:
    if (iter != subObjectives.end()) {
      (*iter)->update();
      if ((*iter)->isDone()) {
        iter++;
      }
    }
    break;
  case OBJECTIVE_TYPE_BUILD_SUBSPAWNER_SUB:
    done = true;
    for (MapUnit::iterator m = getIterator(); m.hasNext(); m++) {
      if (m->type == UNIT_TYPE_SPAWNER) {
        if (m->hp < SUBSPAWNER_UNIT_COST) {
          done = false;
          m->playerDict[psid].objective = this;
          m->setEmptyNeighborScents(strength);
        }
      } else {
        done = false;
        if (m->type == UNIT_TYPE_EMPTY) {
          m->playerDict[psid].objective = this;
          m->setScent(strength);
        }
      }
    }
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
  for (Objective *o : subObjectives)
    delete o;
  subObjectives.clear();
}
