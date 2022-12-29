#ifndef DOOR_H
#define DOOR_H

#include "event.h"

struct Door {
  SpawnerID sid;
  int hp;
  bool isEmpty;
};

#endif
