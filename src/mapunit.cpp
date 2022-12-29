#include "mapunit.h"

#include "agent.h"
#include "constants.h"
#include "door.h"
#include "game.h"

MapUnit::MapUnit(Game* g): type(UNIT_TYPE_OUTSIDE), game(g), marked(true) {
  initializeScents();
  diffusion = 0.0;
}

MapUnit::MapUnit(Game* g, int x_, int y_):
  x(x_), y(y_), type(UNIT_TYPE_EMPTY), game(g), marked(false)  {
  index = y * game->getSize() + x;
  initializeScents();
}

void MapUnit::iterator::next() {
  current = current->right;
  j++;
  if (current->type == UNIT_TYPE_OUTSIDE || j == w) {
    firstInRow = firstInRow->down;
    current = firstInRow;
    j = 0;
    i++;
    if (current->type == UNIT_TYPE_OUTSIDE || i == h) {
      hasNextUnit = false;
    }
  }
}

void MapUnit::initializeScents() {
  scent = 0.0;
  prevScent = 0.0;
  diffusion = 0.15;
}

void MapUnit::clearScent() {
  scent = 0.0;
  prevScent = 0.0;
}

void MapUnit::setScent(double s) {
  if (type == UNIT_TYPE_DOOR && door->sid == game->getPlayerSpawnID() && door->hp == MAX_DOOR_HEALTH && door->isEmpty) scent = s;
  if (type == UNIT_TYPE_EMPTY) scent = s;
}

void MapUnit::setEmptyNeighborScents(double s) {
  MapUnit *neighbors[4] = {
    left,
    right,
    up,
    down
  };
  for (MapUnit *m: neighbors) {
    m->setScent(s);
  }
}

void MapUnit::mark() {
  marked = true;
}

bool MapUnit::isMarked() {
  return marked;
}

void MapUnit::update() {
  prevScent = scent;
  if (type == UNIT_TYPE_EMPTY) {
    diffusion = 0.15;
  } else if (type == UNIT_TYPE_DOOR && door->sid == game->getPlayerSpawnID() && door->hp == MAX_DOOR_HEALTH && door->isEmpty) {
    diffusion = 0.15;
  } else {
    diffusion = 0.0;
  }
  // Left and up have already been iterated through while updating
  scent = diffusion*(left->prevScent + up->prevScent + right->scent + down->scent);
}

MapUnit::~MapUnit() {
  if (door != nullptr) delete door;
}
