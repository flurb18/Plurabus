#ifndef EVENT_H
#define EVENT_H

typedef unsigned int AgentID;

typedef enum _SpawnerID {
  SPAWNER_ID_GREEN,
  SPAWNER_ID_RED
} SpawnerID;

typedef enum AgentAction {
  AGENT_ACTION_STAY,
  AGENT_ACTION_MOVE,
  AGENT_ACTION_BUILDWALL,
  AGENT_ACTION_BUILDDOOR,
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
  unsigned short x, y;
  bool created;
} SpawnerEvent;

typedef struct Events {
  SpawnerEvent spawnEvent;
  AgentEvent agentEvents[0];
} Events;

#endif
