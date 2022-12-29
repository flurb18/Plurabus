#ifndef BUILDING_H
#define BUILDING_H


enum BuildingType {
  BUILDING_TYPE_WALL;
  BUILDING_TYPE_DOOR;
  BUIDLING_TYPE_SPAWNER;
};

struct Building {
  int hp;
  int numAgentsToBuild;
  int numAgentsAdded;
  BuildingType type;
}

#endif
