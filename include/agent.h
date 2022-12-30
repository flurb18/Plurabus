#ifndef AGENT_H
#define AGENT_H

#include "event.h"

/* Forward Declarations */
class Game;
struct MapUnit;

class Agent {
  friend class Spawner;
  friend class Game;
  friend class Building;
  friend class Tower;
private:
  AgentID id;
  SpawnerID sid;
  bool canMoveTo(MapUnit*);
  void die();
public:
  Game* game;
  MapUnit* unit;
  Agent(Game*, MapUnit*, AgentID, SpawnerID);
  ~Agent();
  void update(AgentEvent*);
  SpawnerID getSpawnID();
};

#endif
