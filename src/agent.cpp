#include "agent.h"

#include <random>

#include "constants.h"
#include "game.h"
#include "mapunit.h"

Agent::Agent(Game* g, MapUnit* m,  AgentID i, SpawnerID s):
  id(i), sid(s), game(g), unit(m)  {}

/* should always call delete agt after agt.die() */
void Agent::die() {
  if (unit->type == UNIT_TYPE_AGENT) {
    unit->type = UNIT_TYPE_EMPTY;
  } else if (unit->type == UNIT_TYPE_DOOR) {
    unit->door->isEmpty = true;
  }
  unit->agent = nullptr;
  game->agentDict.erase(id);
  if (sid == game->getPlayerSpawnID())
    game->numPlayerAgents--;
}

/* Update agent based on objective */
void Agent::update(AgentEvent *aevent) {
  aevent->id = id;
  AgentDirection dirRef[5] = {
    AGENT_DIRECTION_LEFT,
    AGENT_DIRECTION_RIGHT,
    AGENT_DIRECTION_UP,
    AGENT_DIRECTION_DOWN,
    AGENT_DIRECTION_STAY
  };
  MapUnit* neighbors[5] = {unit->left, unit->right, unit->up, unit->down, unit};
  // Handle objectives at the neighbors of the agent
  for (int i = 0; i < 5; i++) {
    MapUnit *m = neighbors[i];
    if ((m->type != UNIT_TYPE_OUTSIDE) && (m->objective != nullptr) && (!m->isMarked())) {
      switch (m->objective->type) {
      case OBJECTIVE_TYPE_BUILD_WALL_SUB:
	if (canMoveTo(m) && m->type != UNIT_TYPE_DOOR) {
	  aevent->dir = dirRef[i];
	  aevent->action = AGENT_ACTION_BUILDWALL;
	  m->mark();
	  return;
	}
	break;
      case OBJECTIVE_TYPE_BUILD_SUBSPAWNER_SUB:
	if (m->type == UNIT_TYPE_EMPTY || (m->type == UNIT_TYPE_SPAWNER && m->hp < SUBSPAWNER_UNIT_COST)) {
	  aevent->dir = dirRef[i];
	  aevent->action = AGENT_ACTION_BUILDSUBSPAWNER;
	  m->mark();
	  return;
	}
	break;
      case OBJECTIVE_TYPE_BUILD_TOWER:
	if (m->type == UNIT_TYPE_EMPTY && m->objective->regionIsReadyForBuilding()) {
	  aevent->dir = dirRef[i];
	  aevent->action = AGENT_ACTION_BUILDTOWER;
	  for (MapUnit::iterator it = m->objective->getIterator(); it.hasNext(); it++) {
	    it->mark();
	  }
	  return;
	}
	if  (m->type == UNIT_TYPE_BUILDING && m->building->sid == sid) {
	  aevent->dir = dirRef[i];
	  aevent->action = AGENT_ACTION_BUILDTOWER;
	  m->mark();
	  return;
	}
	break;
      case OBJECTIVE_TYPE_BUILD_BOMB:
	if (m->type == UNIT_TYPE_EMPTY && m->objective->regionIsReadyForBuilding()) {
	  aevent->dir = dirRef[i];
	  aevent->action = AGENT_ACTION_BUILDBOMB;
	  for (MapUnit::iterator it = m->objective->getIterator(); it.hasNext(); it++) {
	    it->mark();
	  }
	  return;
	}
	if  (m->type == UNIT_TYPE_BUILDING && m->building->sid == sid) {
	  aevent->dir = dirRef[i];
	  aevent->action = AGENT_ACTION_BUILDBOMB;
	  m->mark();
	  return;
	}
	break;
      case OBJECTIVE_TYPE_BUILD_DOOR:
	switch(m->type) {
	case UNIT_TYPE_WALL:
	  aevent->dir = dirRef[i];
	  aevent->action = AGENT_ACTION_BUILDDOOR;
	  m->mark();
	  return;
	case UNIT_TYPE_DOOR:
	  if (m->door->sid == sid && m->door->hp < MAX_DOOR_HEALTH) {
	    aevent->dir = dirRef[i];
	    aevent->action = AGENT_ACTION_BUILDDOOR;
	    m->mark();
	    return;
       	  }
	  break;
	default:
	  break;
	}
	break;
      case OBJECTIVE_TYPE_ATTACK:
	switch (m->type) {
	case UNIT_TYPE_WALL:
	  aevent->dir = dirRef[i];
	  aevent->action = AGENT_ACTION_ATTACK;
	  m->mark();
	  return;
	case UNIT_TYPE_AGENT:
	  if (m->agent->getSpawnID() != sid) {
	    aevent->dir = dirRef[i];
	    aevent->action = AGENT_ACTION_ATTACK;
	    m->mark();
	    return;
	  }
	  break;
	case UNIT_TYPE_SPAWNER:
	  aevent->dir = dirRef[i];
	  aevent->action = AGENT_ACTION_ATTACK;
	  m->mark();
	  return;
	case UNIT_TYPE_DOOR:
	  if (m->door->sid != sid) {
	    aevent->dir = dirRef[i];
	    aevent->action = AGENT_ACTION_ATTACK;
	    m->mark();
	    return;
	  }
	  break;
	case UNIT_TYPE_BUILDING:
	  aevent->dir = dirRef[i];
	  aevent->action = AGENT_ACTION_ATTACK;
	  m->mark();
	  return;
	  break;
	default:
	  break;
	}
	break;
      default:
	break;
      }
    }
  }
  // Code for choosing a scent at random (weighted)
  MapUnit *unitOpts[4] = {
    unit->left,
    unit->right,
    unit->up,
    unit->down
  };
  double scents[4];
  for (int i = 0; i < 4; i++) {
    scents[i] = unitOpts[i]->scent;
  }
    /* Do a weighted random selection of where to go, based on the scent in each
    square */
  double total = 0.0;
  for (int i = 0; i < 4; i++) total += scents[i];
  int choice = rand() % 4;
  double rnd = ((double)rand() / (double)RAND_MAX) * total;
  if (total > 0.0 && rnd > 0.0) {
    for(int i=0; i<4; i++) {
      if(rnd < scents[i]) {
	choice = i;
	break;
      }
      rnd -= scents[i];
    }
  }
  if (canMoveTo(unitOpts[choice])) {
    aevent->dir = dirRef[choice];
    aevent->action = AGENT_ACTION_MOVE;
    unitOpts[choice]->mark();
  } else {
    aevent->action = AGENT_ACTION_STAY;
    aevent->dir = AGENT_DIRECTION_STAY;
  }
}

bool Agent::canMoveTo(MapUnit* destUnit) {
  if (destUnit->isMarked()) return false;
  if (destUnit == unit) return true;
  if (destUnit->type == UNIT_TYPE_DOOR) {
    return (destUnit->door->sid == sid && destUnit->door->hp == MAX_DOOR_HEALTH && destUnit->door->isEmpty);
  }
  return (destUnit->type == UNIT_TYPE_EMPTY);
}

SpawnerID Agent::getSpawnID() {
  return sid;
}

Agent::~Agent() {}
