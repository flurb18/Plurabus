#ifndef MAPUNIT_H
#define MAPUNIT_H

#include <map>
#include <list>

#include "event.h"

typedef enum UnitType {
  UNIT_TYPE_EMPTY,
  UNIT_TYPE_BUILDING,
  UNIT_TYPE_AGENT,
  UNIT_TYPE_SPAWNER,
  UNIT_TYPE_WALL,
  UNIT_TYPE_DOOR,
  UNIT_TYPE_OUTSIDE
} UnitType;

class Agent;
class Building;
class Game;
struct Objective;
class Spawner;

struct Door {
  SpawnerID sid;
  int hp;
  bool isEmpty;
};

typedef struct MapUnitPerPlayerData {
  Objective* objective;
  double scent;
  double prevScent;
  double diffusion;
} MapUnitPerPlayerData;

struct MapUnit {
  /* An iterator for traversing through a predefined rectangle of mapunits */
  class iterator {
  public:
    bool hasNextUnit;
    MapUnit* current;
    MapUnit* firstInRow;
    int j, i, w, h;
    iterator(MapUnit* first, int w_, int h_): hasNextUnit(true), \
    current(first), firstInRow(first), j(0), i(0), w(w_), h(h_){};
    iterator operator++() {iterator it = *this; next(); return it;};
    iterator operator++(int junk) {next(); return *this;};
    MapUnit& operator*() {return *current;};
    MapUnit* operator->() {return current;};
    void next();
    bool hasNext() {return hasNextUnit;};
  };
  unsigned int x, y, index;
  int hp;
  UnitType type;
  std::map<SpawnerID, MapUnitPerPlayerData> playerDict;
  Agent* agent;
  Game* game;
  Door *door;
  Building *building;
  bool marked;
  MapUnit* up;
  MapUnit* down;
  MapUnit* left;
  MapUnit* right;
  MapUnit(Game*);
  MapUnit(Game*, int, int);
  void setScent(double);
  void setEmptyNeighborScents(double);
  void clearScent();
  bool isMarked();
  void mark();
  /* Create an iterator through a rectangle of mapunits starting with this one
     at the top left */
  iterator getIterator(int w, int h) {return iterator(this, w, h);};
  void update();
  ~MapUnit();
};

class concentric_iterator {
public:
  std::list<MapUnit*> current;
  std::list<MapUnit*> past;
  int r, R, x, y, w, h;
  Game *g;
  concentric_iterator(Game*, int, int, int, int);
  concentric_iterator operator++() {concentric_iterator it = *this; next(); return it;};
  concentric_iterator operator++(int junk) {next(); return *this;};
  bool hasNext() {return (r > 0);};
  void next();
};

#endif
