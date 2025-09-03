#ifndef MAPUNIT_H
#define MAPUNIT_H

#include <map>
#include <list>
#include <algorithm>

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

typedef struct MapUnitPerPlayerData {
  Objective* objective;
  double scent;
  double prevScent;
  double diffusion;
} MapUnitPerPlayerData;

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
private:
  std::list<MapUnit*> current;
  std::list<MapUnit*> past;
  void next();
public:
  int r, R, x, y, w, h;
  Game *g;
  concentric_iterator(Game *game, int tx, int ty, int tw, int th): x(tx), y(ty), w(tw), h(th) {
    g = game;
    R = (std::min(w,h)-1)/2;
    r = R;
    for (MapUnit::iterator m = g->mapUnitAt(x+R,y+R)->getIterator(w-(2*R),h-(2*R)); m.hasNext(); m++) {
      current.push_back(m.current);
    }
  };
  concentric_iterator operator++() {concentric_iterator it = *this; next(); return it;};
  concentric_iterator operator++(int junk) {next(); return *this;};
  bool hasNext() {return (r > 0);};
}

#endif
