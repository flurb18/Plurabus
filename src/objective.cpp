#include "objective.h"

#include "constants.h"
#include "door.h"
#include "game.h"
#include "spawner.h"

Objective::Objective(ObjectiveType t, int s, Game* g, SDL_Rect r): \
  type(t), strength(s),  done(false), game(g), region(r) {
  switch(type) {
    case OBJECTIVE_TYPE_BUILD_WALL:
      for (int i = 0; region.w - (2*i) > 0 && region.h - (2*i) > 0; i++) {
	SDL_Rect sub = { region.x + i, region.y + i, region.w - (2*i), region.h - (2*i) };
	subObjectives.emplace_front(OBJECTIVE_TYPE_BUILD_WALL_SUB, strength, game, sub); 
      }
      iter = subObjectives.begin();
      break;
  default:
    break;
  }
}

MapUnit::iterator Objective::getIterator() {
  MapUnit* first = game->mapUnitAt(region.x, region.y);
  return first->getIterator(region.w, region.h);
}

bool Objective::isDone() {
  switch(type) {
  case OBJECTIVE_TYPE_BUILD_WALL:
    return (iter == subObjectives.end());
  case OBJECTIVE_TYPE_BUILD_WALL_SUB:
    return done;
  case OBJECTIVE_TYPE_BUILD_DOOR:
    return done;
  default:
    return false;
  }
}

void Objective::update() {
  switch(type) {
  case OBJECTIVE_TYPE_BUILD_WALL:
    if (iter != subObjectives.end()) {
      iter->update();
      if (iter->isDone()){
	iter++;
      }
    }
    break;
  case OBJECTIVE_TYPE_BUILD_WALL_SUB:
    done = true;
    for (MapUnit::iterator m = getIterator(); m.hasNext(); m++) {
      if (m->type == UNIT_TYPE_EMPTY) {
	done = false;
	m->objective = this;
	m->setScent(strength);
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
	  m->objective = this;
	  m->setEmptyNeighborScents(strength);
	}
	break;
      case UNIT_TYPE_SPAWNER:
	if (m->spawner->getSpawnID() != game->getPlayerSpawnID()) {
	  done = false;
	  m->objective = this;
	  m->setEmptyNeighborScents(strength);
	}
	break;
      case UNIT_TYPE_DOOR:
	if (m->door->sid != game->getPlayerSpawnID()) {
	  done = false;
	  m->objective = this;
	  m->setEmptyNeighborScents(strength);
	}
	break;
      case UNIT_TYPE_WALL:
	done = false;
	m->objective = this;
	m->setEmptyNeighborScents(strength);
	break;
      default:
	break;
      }
    }
    break;
  case OBJECTIVE_TYPE_GOTO:
    for (MapUnit::iterator m = getIterator(); m.hasNext(); m++) {
      m->setScent(strength);
    }
  case OBJECTIVE_TYPE_BUILD_DOOR:
    done = true;
    for (MapUnit::iterator m = getIterator(); m.hasNext(); m++) {
      switch(m->type) {
      case UNIT_TYPE_DOOR:
	if (m->door->hp < MAX_DOOR_HEALTH) {
	  done = false;
	  m->objective = this;
	  m->setEmptyNeighborScents(strength);
	}
	break;
      case UNIT_TYPE_WALL:
	done = false;
	m->objective = this;
	m->setEmptyNeighborScents(strength);
	break;
      default:
	break;
      }
    }
  default:
    break;
  }
}

Objective::~Objective() {
  for (MapUnit::iterator m = getIterator(); m.hasNext(); m++) {
    if (m->objective == this) m->objective = nullptr;
  }
  subObjectives.clear();
}
