#ifndef EVENT_H
#define EVENT_H

#include "constants.h"

typedef unsigned int AgentID;

typedef enum _SpawnerID {
  SPAWNER_ID_ONE,
  SPAWNER_ID_TWO
} SpawnerID;

typedef enum AgentAction {
  AGENT_ACTION_STAY,
  AGENT_ACTION_MOVE,
  AGENT_ACTION_BUILDWALL,
  AGENT_ACTION_BUILDDOOR,
  AGENT_ACTION_BUILDSUBSPAWNER,
  AGENT_ACTION_BUILDTOWER,
  AGENT_ACTION_BUILDBOMB,
  AGENT_ACTION_ATTACK
} AgentAction;

typedef enum AgentDirection {
  AGENT_DIRECTION_LEFT,
  AGENT_DIRECTION_RIGHT,
  AGENT_DIRECTION_UP,
  AGENT_DIRECTION_DOWN,
  AGENT_DIRECTION_STAY
} AgentDirection;
  
typedef struct AgentEvent {
  AgentID id;
  AgentDirection dir;
  AgentAction action;
} AgentEvent;

typedef struct SpawnerEvent {
  AgentID id;
  SpawnerID sid;
  int x, y;
  bool created;
} SpawnerEvent;

typedef struct TowerEvent {
  AgentID id;
  int x, y;
  bool destroyed;
} TowerEvent;

typedef struct BombEvent {
  int x,y;
  bool detonated;
} BombEvent;

typedef struct Events {
  SpawnerEvent spawnEvents[MAX_SUBSPAWNERS+1];
  TowerEvent towerEvents[MAX_TOWERS];
  BombEvent bombEvents[MAX_BOMBS];
  int numAgentEvents;
  AgentEvent agentEvents[0];
} Events;

#endif
