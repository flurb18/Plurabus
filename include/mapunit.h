#ifndef MAPUNIT_H
#define MAPUNIT_H

#include "event.h"

enum UnitType {
  UNIT_TYPE_EMPTY,
  UNIT_TYPE_BUILDING,
  UNIT_TYPE_AGENT,
  UNIT_TYPE_SPAWNER,
  UNIT_TYPE_WALL,
  UNIT_TYPE_DOOR,
  UNIT_TYPE_OUTSIDE
};

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

struct MapUnit {
  /* An iterator for traversing through a predefined rectangle of mapunits */
  class iterator {
  private:
    bool hasNextUnit;
    MapUnit* current;
    MapUnit* firstInRow;
    void next();
  public:
    int j, i, w, h;
    iterator(MapUnit* first, int w_, int h_): hasNextUnit(true), \
    current(first), firstInRow(first), j(0), i(0), w(w_), h(h_){};
    iterator operator++() {iterator it = *this; next(); return it;};
    iterator operator++(int junk) {next(); return *this;};
    MapUnit& operator*() {return *current;};
    MapUnit* operator->() {return current;};
    bool hasNext() {return hasNextUnit;};
  };
  unsigned int x, y, index;
  int hp;
  UnitType type;
  double scent;
  double prevScent;
  // Should be less than 0.25
  double diffusion;
  Agent* agent;
  Game* game;
  Door *door;
  Building *building;
  bool marked;
  Objective* objective;
  MapUnit* up;
  MapUnit* down;
  MapUnit* left;
  MapUnit* right;
  MapUnit(Game*);
  MapUnit(Game*, int, int);
  void initializeScents();
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

#endif
