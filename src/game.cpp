#include "game.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <pthread.h>
#include <thread>
#include <chrono>
#include <ctime>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_rect.h>
#include <iostream>
#include <random>
#include <stdlib.h>
#include <string>

#include "agent.h"
#include "constants.h"
#include "display.h"
#include "event.h"
#include "mapunit.h"
#include "menu.h"
#include "nethandler.h"
#include "panel.h"

/*------------------CONTENTS----------------------*/
/*------------------------------------------------*/
/*-----1. Constructor / Destructor----------------*/
/*-----2. Main net thread-------------------------*/
/*-----3. Game state functions--------------------*/
/*-----4. Objective functions---------------------*/
/*-----5. Interface functions---------------------*/
/*-----6. Display functions-----------------------*/
/*-----7. Helper functions & one-liners-----------*/
/*-----8. Main event loop-------------------------*/
/*------------------------------------------------*/

/*---------Constructor / Destructor----------------*/

Game::Game(int gm, int sz, int psz, double scl, char *pstr, char *uri, bool mob)
    : pairString(pstr), mobile(mob), resignConfirmation(false), ended(false),
      eventsBufferCapacity(INIT_EVENT_BUFFER_SIZE),
      context(GAME_CONTEXT_CONNECTING),
      selectionContext(SELECTION_CONTEXT_UNSELECTED), initScale(scl),
      scale(scl), gameMode(gm), gameSize(sz), panelSize(psz), mouseX(0),
      mouseY(0), placementW(0), placementH(0), zapCounter(1),
      secondsRemaining(GAME_TIME_SECONDS + STARTUP_TIME_SECONDS),
      doneStatus(DONE_STATUS_INIT), newAgentID(1), outside(this),
      selectedObjective(nullptr) {

  numPlayerAgents[SPAWNER_ID_ONE] = 0;
  numPlayerAgents[SPAWNER_ID_TWO] = 0;
  panelYDrawOffset = (mobile ? panelSize : 0);
  eventsBuffer = malloc(messageSize(eventsBufferCapacity));
  gameDisplaySize = scaleInt(gameSize);
  mapUnits.reserve(gameSize * gameSize);
  for (int i = 0; i < gameSize; i++) {
    for (int j = 0; j < gameSize; j++) {
      mapUnits.push_back(new MapUnit(this, j, i));
    }
  }
  for (int i = 0; i < gameSize; i++) {
    for (int j = 0; j < gameSize; j++) {
      int index = i * gameSize + j;
      if (i == 0)
        mapUnits[index]->up = &outside;
      else
        mapUnits[index]->up = mapUnits[index - gameSize];
      if (i == gameSize - 1)
        mapUnits[index]->down = &outside;
      else
        mapUnits[index]->down = mapUnits[index + gameSize];
      if (j == 0)
        mapUnits[index]->left = &outside;
      else
        mapUnits[index]->left = mapUnits[index - 1];
      if (j == gameSize - 1)
        mapUnits[index]->right = &outside;
      else
        mapUnits[index]->right = mapUnits[index + 1];
    }
  }

  selection = {0, 0, 1, 1};
  menuSize = gameDisplaySize / MENU_ITEMS_IN_VIEW;
  int fontSize = gameDisplaySize / FONT_DIVISOR;
  disp = new Display(gameDisplaySize, menuSize, panelSize, fontSize, mobile);

#ifdef ANDROID
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  env->GetJavaVM(&jvm);
#endif

  panel = new Panel(disp);
  std::string welcomeText = "Welcome to " + std::string(TITLE) + "!";
  panel->addText(welcomeText.c_str());
  view = {0, 0, gameSize, gameSize};
  selectedUnit = mapUnitAt(0, 0);
  menu = new Menu(this);
  int p1offset = gameSize - SPAWNER_PADDING - SPAWNER_SIZE;
  int p2offset = SPAWNER_PADDING;
  buildingLists[BUILDING_TYPE_SPAWNER].push_back(
      new Spawner(this, SPAWNER_ID_ONE, p1offset, p1offset));
  buildingLists[BUILDING_TYPE_SPAWNER].push_back(
      new Spawner(this, SPAWNER_ID_TWO, p2offset, p2offset));
  objectiveInfoTextures[OBJECTIVE_TYPE_ATTACK] =
      disp->cacheTextWrapped("Objective - Attack", 0);
  objectiveInfoTextures[OBJECTIVE_TYPE_GOTO] =
      disp->cacheTextWrapped("Objective - Go To", 0);
  objectiveInfoTextures[OBJECTIVE_TYPE_BUILD_WALL] =
      disp->cacheTextWrapped("Objective - Build Wall", 0);
  objectiveInfoTextures[OBJECTIVE_TYPE_BUILD_DOOR] =
      disp->cacheTextWrapped("Objective - Build Door", 0);
  objectiveInfoTextures[OBJECTIVE_TYPE_BUILD_TOWER] =
      disp->cacheTextWrapped("Objective - Build Tower", 0);
  objectiveInfoTextures[OBJECTIVE_TYPE_BUILD_SUBSPAWNER] =
      disp->cacheTextWrapped("Objective - Build Subspawner", 0);
  objectiveInfoTextures[OBJECTIVE_TYPE_BUILD_BOMB] =
      disp->cacheTextWrapped("Objective - Build Bomb", 0);
  p1bombTexture = nullptr;
  setColors("GREEN", "RED", 0, 255, 0, 255, 0, 0);
  pthread_mutex_init(&threadLock, NULL);
  switch(gameMode) {
    case 0:
      net = new NetHandler(this, pairString, uri);
      break;
    case 1:
      simpleAggMode();
      break;
    case 2:
      simpleDefMode();
      break;
  }
}

Game::~Game() {
  if (!ended)
    end(DONE_STATUS_EXIT);
  if (gameMode == 0) {
    delete net;
  }
  for (MapUnit *u : mapUnits) {
    u->left = nullptr;
    u->right = nullptr;
    u->down = nullptr;
    u->up = nullptr;
  }
  for (unsigned int i = 0; i < mapUnits.size(); i++) {
    delete mapUnits[i];
  }
  for (auto it = agentDict.begin(); it != agentDict.end(); it++) {
    delete it->second;
  }
  for (auto it = buildingLists.begin(); it != buildingLists.end(); it++) {
    for (Building *b : it->second) {
      delete b;
    }
    it->second.clear();
  }
  for (auto it = objectiveInfoTextures.begin();
       it != objectiveInfoTextures.end(); it++) {
    SDL_DestroyTexture(it->second);
  }
  towerZaps.clear();
  bombEffects.clear();
  SDL_DestroyTexture(p1bombTexture);
  SDL_DestroyTexture(p2bombTexture);
  objectiveInfoTextures.clear();
  agentDict.clear();
  buildingLists.clear();
  mapUnits.clear();
  delete disp;
  delete menu;
  delete panel;
  free(eventsBuffer);
  pthread_mutex_destroy(&threadLock);
}

void Game::end(DoneStatus s) {
  if (ended) return;
  context = GAME_CONTEXT_DONE;
  ended = true;
  std::string closeText = "Connection closed.";
  int p1units, p2units;
  bool we_have_a_winner = false;
  switch (s) {
  case DONE_STATUS_WINRECV:
  case DONE_STATUS_WINNER:
    we_have_a_winner = true;
    switch (winnerSpawnID) {
    case SPAWNER_ID_ONE:
      closeText = colorScheme.p1name + " team wins!";
      if (s == DONE_STATUS_WINNER) net->sendText("WINNER_1");
      break;
    case SPAWNER_ID_TWO:
      closeText = colorScheme.p2name + " team wins!";
      if (s == DONE_STATUS_WINNER) net->sendText("WINNER_2");
      break;
    }
    break;
  case DONE_STATUS_DRAW:
    closeText = "Draw!";
    break;
  case DONE_STATUS_DISCONNECT:
    closeText = "Other player disconnected.";
    break;
  case DONE_STATUS_RESIGN:
    we_have_a_winner = true;
    if (winnerSpawnID != playerSpawnID && gameMode == 0) {
      net->sendText("RESIGN");
    }
    switch (winnerSpawnID) {
    case SPAWNER_ID_ONE:
      closeText = colorScheme.p1name + " team wins by resignation!";
      break;
    case SPAWNER_ID_TWO:
      closeText = colorScheme.p2name + " team wins by resignation!";
      break;
    }
    break;
  case DONE_STATUS_EXIT:
    if (gameMode == 0) {
      net->sendText("DISCONNECT");
    }
    break;
  case DONE_STATUS_OTHER:
    closeText = "What! You shouldn't see this text!";
    break;
  case DONE_STATUS_TIMEOUT:
    Building *p1spawn;
    Building *p2spawn;
    for (Building *build : buildingLists[BUILDING_TYPE_SPAWNER]) {
      if (build->sid == SPAWNER_ID_ONE) {
        p1spawn = build;
      }
      if (build->sid == SPAWNER_ID_TWO) {
        p2spawn = build;
      }
    }
    p1units = ((Spawner *)p1spawn)->getNumSpawnUnits();
    p2units = ((Spawner *)p2spawn)->getNumSpawnUnits();
    panel->addText("Timeout!");
    if (p1units == p2units) {
      closeText =
          "Draw! Both player have " + std::to_string(p1units) + " units left.";
    } else {
      we_have_a_winner = true;
      if (p1units > p2units) {
        winnerSpawnID = p1spawn->sid;
        closeText = colorScheme.p1name + " team wins! They had " +
                    std::to_string(p1units) + " Spawner units left, while " +
                    colorScheme.p2name + " team had " +
                    std::to_string(p2units) + ".";
      } else {
        winnerSpawnID = p2spawn->sid;
        closeText = colorScheme.p2name + " team wins! They had " +
                    std::to_string(p2units) + " Spawner units left, while " +
                    colorScheme.p1name + " team had " +
                    std::to_string(p1units) + ".";
      }
    }
    if (gameMode == 0) {
      net->sendText("TIMEOUT");
    }
    break;
  case DONE_STATUS_FRAME_TIMEOUT:
    closeText = "Network error, took too long.";
    break;
  case DONE_STATUS_BACKGROUND:
    if (gameMode == 0) {
      net->sendText("DISCONNECT");
    }
    break;
  default:
    break;
  }
  if (gameMode == 0) {
    net->closeConnection("Normal");
  }
  panel->addText(closeText.c_str());
  int p1score, p2score;
  p1score = 0;
  p2score = 0;
  if (we_have_a_winner) {
    if (winnerSpawnID == playerSpawnID) {
      p1score = 1;
    } else {
      p2score = 1;
    }
  }
  std::string returnText =
      std::to_string(p1score) + "-" + std::to_string(p2score);
  std::cout << returnText << std::endl;
}

/*--------------Game state functions-------------*/

void Game::checkSpawnersDestroyed() {
  auto ssit = buildingLists[BUILDING_TYPE_SUBSPAWNER].begin();
  while (ssit != buildingLists[BUILDING_TYPE_SUBSPAWNER].end()) {
    if (((Subspawner *)(*ssit))->isDestroyed()) {
      for (MapUnit::iterator m = (*ssit)->getIterator(); m.hasNext(); m++) {
        m->building = nullptr;
      }
      delete (*ssit);
      ssit = buildingLists[BUILDING_TYPE_SUBSPAWNER].erase(ssit);
    } else
      ssit++;
  }
  for (Building *build : buildingLists[BUILDING_TYPE_SPAWNER]) {
    Spawner *s = (Spawner *)build;
    if (s->isDestroyed()) {
      switch (s->sid) {
      case SPAWNER_ID_ONE:
        winnerSpawnID = SPAWNER_ID_TWO;
        break;
      case SPAWNER_ID_TWO:
        winnerSpawnID = SPAWNER_ID_ONE;
        break;
      }
      end(DONE_STATUS_WINNER);
    }
  }
}

void Game::deleteMarkedAgents() {
  for (AgentID id : markedAgents) {
    auto it = agentDict.find(id);
    if (it == agentDict.end())
      continue;
    Agent *a = it->second;
    MapUnit *u = a->unit;
    SpawnerID s = a->sid;
    if (u->type == UNIT_TYPE_AGENT) {
      u->type = UNIT_TYPE_EMPTY;
    } else if (u->type == UNIT_TYPE_DOOR) {
      u->door->isEmpty = true;
    }
    u->agent = nullptr;
    agentDict.erase(it);
    delete a;
    numPlayerAgents[s]--;
  }
  markedAgents.clear();
}

void Game::deleteMarkedBuildings() {
  std::deque<Building*> past;
  for (Building *build : markedBuildings) {
    bool found_past = false;
    for (Building *pastbuild : past) {
      if (build == pastbuild) {
        found_past = true;
        break;
      }
    }
    if (found_past) continue;
    past.push_front(build);
  }
  for (Building *build : past) {
    for (MapUnit::iterator it = build->getIterator(); it.hasNext(); it++) {
      it->type = UNIT_TYPE_EMPTY;
      it->building = nullptr;
    }
    for (auto it = buildingLists[build->type].begin();
          it != buildingLists[build->type].end(); it++) {
      Building *b = *it;
      if (rectCollides(build->region, b->region)) {
        it = buildingLists[build->type].erase(it);
        break;
      }
    }
    delete build;
  }
  markedBuildings.clear();
  past.clear();
}

int Game::messageSize(int n) {
  return (sizeof(SpawnerEvent) * (MAX_SUBSPAWNERS + 1)) +
         (sizeof(TowerEvent) * MAX_TOWERS) + (sizeof(BombEvent) * MAX_BOMBS) +
         sizeof(int) + (sizeof(AgentEvent) * n);
}

void Game::receiveData(void *data, int numBytes) {
  Events *events = (Events *)data;
  sizeEventsBuffer(events->numAgentEvents);
  memcpy(eventsBuffer, (const void *)data, messageSize(events->numAgentEvents));
  receiveEventsBuffer();
  update();
  receiveEventsBuffer();
  sendEventsBuffer();
}

void Game::receiveEventsBuffer() {
  Events *events = (Events *)eventsBuffer;
  for (int i = 0; i < events->numAgentEvents; i++) {
    receiveAgentEvent(&events->agentEvents[i]);
  }
  for (int i = 0; i < MAX_TOWERS; i++) {
    receiveTowerEvent(&events->towerEvents[i]);
  }
  for (int i = 0; i < MAX_BOMBS; i++) {
    receiveBombEvent(&events->bombEvents[i]);
  }
  for (int i = 0; i < MAX_SUBSPAWNERS + 1; i++) {
    receiveSpawnerEvent(&events->spawnEvents[i]);
  }
  deleteMarkedAgents();
  deleteMarkedBuildings();
  checkSpawnersDestroyed();
}

void Game::sendEventsBuffer() {
  Events *events = (Events *)eventsBuffer;
  net->send(eventsBuffer, messageSize(events->numAgentEvents));
}

void Game::sizeEventsBuffer(int s) {
  bool resize = (s > eventsBufferCapacity);
  while (s > eventsBufferCapacity)
    eventsBufferCapacity *= 2;
  if (resize) {
    free(eventsBuffer);
    eventsBuffer = malloc(messageSize(eventsBufferCapacity));
  }
}

void Game::receiveAgentEvent(AgentEvent *aevent) {
  auto it = agentDict.find(aevent->id);
  if (it == agentDict.end())
    return;
  Agent *a = it->second;
  int x, y, count;
  SpawnerID s;
  Building *build;
  MapUnit *startuptr = a->unit;
  MapUnit *destuptr;
  MapUnit *first;
  switch (aevent->dir) {
  case AGENT_DIRECTION_LEFT:
    destuptr = startuptr->left;
    break;
  case AGENT_DIRECTION_RIGHT:
    destuptr = startuptr->right;
    break;
  case AGENT_DIRECTION_UP:
    destuptr = startuptr->up;
    break;
  case AGENT_DIRECTION_DOWN:
    destuptr = startuptr->down;
    break;
  case AGENT_DIRECTION_STAY:
    destuptr = startuptr;
    break;
  }
  switch (aevent->action) {
  case AGENT_ACTION_MOVE:
    if (destuptr->type == UNIT_TYPE_DOOR) destuptr->door->isEmpty = false;
    else destuptr->type = UNIT_TYPE_AGENT;
    a->unit = destuptr;
    destuptr->agent = a;
    if (startuptr->type == UNIT_TYPE_DOOR) startuptr->door->isEmpty = true;
    else startuptr->type = UNIT_TYPE_EMPTY;
    startuptr->agent = nullptr;
    break;
  case AGENT_ACTION_BUILDWALL:
    destuptr->type = UNIT_TYPE_WALL;
    destuptr->hp = STARTING_WALL_HEALTH;
    markAgentForDeletion(a->id);
    break;
  case AGENT_ACTION_BUILDDOOR:
    if (destuptr->type == UNIT_TYPE_WALL) {
      destuptr->type = UNIT_TYPE_DOOR;
      destuptr->door = new Door();
      destuptr->door->sid = a->sid;
      destuptr->door->hp = 1;
      destuptr->door->isEmpty = false;
    } else {
      destuptr->door->hp++;
      if (destuptr->door->hp == MAX_DOOR_HEALTH)
        destuptr->door->isEmpty = true;
    }
    markAgentForDeletion(a->id);
    break;
  case AGENT_ACTION_BUILDTOWER:
    if (destuptr->type == UNIT_TYPE_EMPTY) {
      x = destuptr->x - TOWER_SIZE / 2;
      y = destuptr->y - TOWER_SIZE / 2;
      first = mapUnitAt(x, y);
      count = 0;
      s = a->sid;
      for (MapUnit::iterator it = first->getIterator(TOWER_SIZE, TOWER_SIZE);
           it.hasNext(); it++) {
        if (it->type == UNIT_TYPE_AGENT) {
          markAgentForDeletion(it->agent->id);
          count++;
        }
      }
      if (count > MAX_TOWER_HEALTH)
        count = MAX_TOWER_HEALTH;
      Tower *tower = new Tower(this, s, x, y);
      tower->hp = count;
      if (s == playerSpawnID)
        destuptr->playerDict[playerSpawnID].objective->started = true;
      buildingLists[BUILDING_TYPE_TOWER].push_back(tower);
    } else {
      destuptr->building->hp++;
      markAgentForDeletion(a->id);
    }
    break;
  case AGENT_ACTION_BUILDBOMB:
    if (destuptr->type == UNIT_TYPE_EMPTY) {
      x = destuptr->x - BOMB_SIZE / 2;
      y = destuptr->y - BOMB_SIZE / 2;
      first = mapUnitAt(x, y);
      count = 0;
      s = a->sid;
      for (MapUnit::iterator it = first->getIterator(BOMB_SIZE, BOMB_SIZE);
           it.hasNext(); it++) {
        if (it->type == UNIT_TYPE_AGENT) {
          markAgentForDeletion(it->agent->id);
          count++;
        }
      }
      if (count > MAX_BOMB_HEALTH)
        count = MAX_BOMB_HEALTH;
      Bomb *bomb = new Bomb(this, s, x, y);
      bomb->hp = count;
      if (s == playerSpawnID)
        destuptr->playerDict[playerSpawnID].objective->started = true;
      buildingLists[BUILDING_TYPE_BOMB].push_back(bomb);
    } else {
      destuptr->building->hp++;
      markAgentForDeletion(a->id);
    }
    break;
  case AGENT_ACTION_BUILDSUBSPAWNER:
    if (destuptr->type == UNIT_TYPE_EMPTY) {
      destuptr->hp = 1;
      destuptr->type = UNIT_TYPE_SPAWNER;
      if (destuptr->building == nullptr) {
        Subspawner *subspawner =
            new Subspawner(this, a->sid, destuptr->x - SUBSPAWNER_SIZE / 2,
                           destuptr->y - SUBSPAWNER_SIZE / 2);
        if (a->sid == playerSpawnID)
          destuptr->playerDict[playerSpawnID].objective->started = true;
        buildingLists[BUILDING_TYPE_SUBSPAWNER].push_back(subspawner);
      }
    } else {
      destuptr->hp++;
    }
    markAgentForDeletion(a->id);
    break;
  case AGENT_ACTION_ATTACK:
    switch (destuptr->type) {
    case UNIT_TYPE_SPAWNER:
      destuptr->type = UNIT_TYPE_EMPTY;
      markAgentForDeletion(a->id);
      break;
    case UNIT_TYPE_AGENT:
      markAgentForDeletion(destuptr->agent->id);
      markAgentForDeletion(a->id);
      break;
    case UNIT_TYPE_WALL:
      destuptr->hp--;
      if (destuptr->hp == 0)
        destuptr->type = UNIT_TYPE_EMPTY;
      markAgentForDeletion(a->id);
      break;
    case UNIT_TYPE_DOOR:
      destuptr->door->hp--;
      if (destuptr->door->hp == 0) {
        delete destuptr->door;
        if (destuptr->agent != nullptr) {
          destuptr->type = UNIT_TYPE_AGENT;
        } else {
          destuptr->type = UNIT_TYPE_EMPTY;
        }
      }
      markAgentForDeletion(a->id);
      break;
    case UNIT_TYPE_BUILDING:
      build = destuptr->building;
      build->hp--;
      if (build->hp <= 0) markBuildingForDeletion(build);
      markAgentForDeletion(a->id);
      break;
    default:
      break;
    }
  default:
    break;
  }
}

void Game::receiveTowerEvent(TowerEvent *tevent) {
  if (tevent->destroyed) {
    auto it = agentDict.find(tevent->id);
    if (it != agentDict.end()) {
      Agent *a = it->second;
      TowerZap t = {tevent->x, tevent->y, (int)a->unit->x, (int)a->unit->y, std::chrono::high_resolution_clock::now()};
      towerZaps.push_back(t);
      markAgentForDeletion(a->id);
    }
  }
}

void Game::receiveSpawnerEvent(SpawnerEvent *sevent) {
  if (sevent->created) {
    MapUnit *uptr = mapUnitAt(sevent->x, sevent->y);
    Agent *a = new Agent(this, uptr, sevent->id, sevent->sid);
    agentDict.insert(std::make_pair(sevent->id, a));
    uptr->agent = a;
    uptr->type = UNIT_TYPE_AGENT;
    if (sevent->id >= newAgentID)
      newAgentID = sevent->id + 1;
    numPlayerAgents[sevent->sid]++;
  }
}

void Game::receiveBombEvent(BombEvent *bevent) {
  if (bevent->detonated) {
    BombEffect be = {bevent->x, bevent->y, std::chrono::high_resolution_clock::now()};
    bombEffects.push_back(be);
    int startx = bevent->x - BOMB_AOE_RADIUS;
    int starty = bevent->y - BOMB_AOE_RADIUS;
    if (startx < 0)
      startx = 0;
    if (starty < 0)
      starty = 0;
    MapUnit *first = mapUnitAt(startx, starty);
    for (MapUnit::iterator m =
             first->getIterator(BOMB_AOE_RADIUS * 2, BOMB_AOE_RADIUS * 2);
         m.hasNext(); m++) {
      int dx = m->x - bevent->x;
      int dy = m->y - bevent->y;
      if (dx * dx + dy * dy <= BOMB_AOE_RADIUS * BOMB_AOE_RADIUS) {
        switch (m->type) {
        case UNIT_TYPE_AGENT:
          markAgentForDeletion(m->agent->id);
          break;
        case UNIT_TYPE_SPAWNER:
          break;
        case UNIT_TYPE_DOOR:
          delete m->door;
          m->door = nullptr;
          if (m->agent != nullptr) {
            markAgentForDeletion(m->agent->id);
          }
          break;
        case UNIT_TYPE_BUILDING:
          markBuildingForDeletion(m->building);
          break;
        default:
          break;
        }
        m->type = UNIT_TYPE_EMPTY;
      }
    }
  }
}

void Game::update() {
  sizeEventsBuffer(numPlayerAgents[playerSpawnID]);
  Events *events = (Events *)eventsBuffer;
  for (MapUnit *u : mapUnits) {
    u->marked = false;
    u->update();
    u->playerDict[playerSpawnID].objective = nullptr;
  }
  auto it = objectives.begin();
  while (it != objectives.end()) {
    if (!((*it)->sid == playerSpawnID)) {
      it++;
      continue;
    }
    (*it)->update();
    if ((*it)->isDone()) {
      if (selectedObjective == *it)
        selectedObjective = nullptr;
      delete *it;
      it = objectives.erase(it);
    } else
      it++;
  }
  int i = 0;
  for (auto it = agentDict.begin(); it != agentDict.end(); it++) {
    if (it->second->sid == playerSpawnID) {
      it->second->update(&events->agentEvents[i]);
      i++;
    }
  }
  for (i = 0; i < MAX_TOWERS; i++)
    events->towerEvents[i].destroyed = false;
  for (i = 0; i < MAX_BOMBS; i++)
    events->bombEvents[i].detonated = false;
  for (i = 0; i < MAX_SUBSPAWNERS + 1; i++)
    events->spawnEvents[i].created = false;
  int bombI = 0;
  int towerI = 0;
  int subspawnerI = 1;
  for (auto it = buildingLists.begin(); it != buildingLists.end(); it++) {
    for (Building *build : it->second) {
      if (build->sid == playerSpawnID) {
        switch (build->type) {
        case BUILDING_TYPE_BOMB:
          ((Bomb *)build)->update(&events->bombEvents[bombI]);
          bombI++;
          break;
        case BUILDING_TYPE_TOWER:
          ((Tower *)build)->update(&events->towerEvents[towerI]);
          towerI++;
          break;
        case BUILDING_TYPE_SUBSPAWNER:
          ((Subspawner *)build)->update(&events->spawnEvents[subspawnerI]);
          subspawnerI++;
          break;
        case BUILDING_TYPE_SPAWNER:
          ((Spawner *)build)->update(&events->spawnEvents[0]);
        }
      }
    }
  }
  events->numAgentEvents = numPlayerAgents[playerSpawnID];
}

void Game::simpleAggMode() {
  int p1offset = gameSize - SPAWNER_PADDING - SPAWNER_SIZE;
  playerSpawnID = SPAWNER_ID_ONE;
  panel->addText("You are the GREEN team.");
  context = GAME_CONTEXT_PRACTICE;
  SDL_Rect green_spawn = {p1offset, p1offset, p1offset + SPAWNER_SIZE,
                          p1offset + SPAWNER_SIZE};
  Objective *o = new Objective(OBJECTIVE_TYPE_ATTACK, 255, this, green_spawn,
                                SPAWNER_ID_TWO);
  objectives.push_back(o);
}

void Game::simpleDefMode() {
  int p1offset = gameSize - SPAWNER_PADDING - SPAWNER_SIZE;
  int p2offset = SPAWNER_PADDING;
  playerSpawnID = SPAWNER_ID_ONE;
  panel->addText("You are the GREEN team.");
  context = GAME_CONTEXT_PRACTICE;
  SDL_Rect green_spawn = {p1offset, p1offset, p1offset + SPAWNER_SIZE,
                          p1offset + SPAWNER_SIZE};
  Objective *attack = new Objective(OBJECTIVE_TYPE_ATTACK, 255, this, green_spawn,
                                SPAWNER_ID_TWO);
  objectives.push_back(attack);
  SDL_Rect defensiveWall = { 0, p2offset + SPAWNER_SIZE + 15, 75, 2 };
  Objective *wall = new Objective(OBJECTIVE_TYPE_BUILD_WALL, 255, this, defensiveWall, SPAWNER_ID_TWO);
  objectives.push_back(wall);
  SDL_Rect subspawn_location = { p2offset + SPAWNER_SIZE + 20, p2offset + (SPAWNER_SIZE/2), SUBSPAWNER_SIZE, SUBSPAWNER_SIZE };
  Objective *subspawn = new Objective(OBJECTIVE_TYPE_BUILD_SUBSPAWNER, 255, this, subspawn_location, SPAWNER_ID_TWO);
  objectives.push_back(subspawn);
}

/*------------------Objective Functions-----------------*/

void Game::clearScent() {
  for (auto it = getSelectionIterator(); it.hasNext(); it++) {
    it->clearScent();
  }
}

void Game::placeBuilding(BuildingType type) {
  int numPlayerBuilds = 0;
  int maxBuilds;
  int s;
  ObjectiveType oType;
  std::string errorEnd;
  switch (type) {
  case BUILDING_TYPE_TOWER:
    maxBuilds = MAX_TOWERS;
    s = TOWER_SIZE;
    oType = OBJECTIVE_TYPE_BUILD_TOWER;
    errorEnd = " towers";
    break;
  case BUILDING_TYPE_BOMB:
    maxBuilds = MAX_BOMBS;
    s = BOMB_SIZE;
    oType = OBJECTIVE_TYPE_BUILD_BOMB;
    errorEnd = " bombs";
    break;
  case BUILDING_TYPE_SUBSPAWNER:
    maxBuilds = MAX_SUBSPAWNERS;
    s = SUBSPAWNER_SIZE;
    oType = OBJECTIVE_TYPE_BUILD_SUBSPAWNER;
    errorEnd = " subspawners";
    break;
  default:
    return;
  }
  for (Building *b : buildingLists[type]) {
    if (b->sid == playerSpawnID)
      numPlayerBuilds++;
  }
  for (Objective *o : objectives) {
    if (o->sid == playerSpawnID && o->type == oType && !o->started)
      numPlayerBuilds++;
  }
  if (numPlayerBuilds < maxBuilds) {
    placementW = s;
    placementH = s;
    placingType = type;
    selectionContext = SELECTION_CONTEXT_PLACING;
    mouseMoved(mouseX, mouseY);
  } else {
    std::string tooMany = "You can only build " + std::to_string(maxBuilds) +
                          errorEnd + " at a time.";
    panel->addText(tooMany.c_str());
  }
}

void Game::setObjective(ObjectiveType oType) {
  bool placingOverride = false;
  if (selectionContext == SELECTION_CONTEXT_PLACING) {
    if (oType == OBJECTIVE_TYPE_BUILD_TOWER && selection.w == TOWER_SIZE &&
        selection.h == TOWER_SIZE)
      placingOverride = true;
    if (oType == OBJECTIVE_TYPE_BUILD_SUBSPAWNER &&
        selection.w == SUBSPAWNER_SIZE && selection.h == SUBSPAWNER_SIZE)
      placingOverride = true;
    if (oType == OBJECTIVE_TYPE_BUILD_BOMB && selection.w == BOMB_SIZE &&
        selection.h == BOMB_SIZE)
      placingOverride = true;
  }
  if (selectionContext == SELECTION_CONTEXT_SELECTED ||
      selectionContext == SELECTION_CONTEXT_SELECTING || placingOverride) {
    Objective *o = new Objective(oType, 255, this, selection, playerSpawnID);
    objectives.push_back(o);
    selectionContext = SELECTION_CONTEXT_UNSELECTED;
  } else {
    panel->addText("You must select a region to designate.");
  }
}

void Game::deleteSelectedObjective() {
  if (menu->getIfObjectivesShown()) {
    if (selectedObjective) {
      for (auto it = objectives.begin(); it != objectives.end(); it++) {
        if (*it == selectedObjective) {
          objectives.erase(it);
          break;
        }
      }
      MapUnit *first =
          mapUnitAt(selectedObjective->region.x, selectedObjective->region.y);
      for (MapUnit::iterator it = first->getIterator(
               selectedObjective->region.w, selectedObjective->region.h);
           it.hasNext(); it++) {
        it->playerDict[playerSpawnID].objective = nullptr;
      }
      delete selectedObjective;
      selectedObjective = nullptr;
    }
  }
}

/*------------Interface functions---------------*/

void Game::toggleShowObjectives() {
  menu->items.at(3)->subMenu.toggleFlags.at(0) =
      !menu->items.at(3)->subMenu.toggleFlags.at(0);
}

void Game::toggleShowScents() {
  menu->items.at(3)->subMenu.toggleFlags.at(1) =
      !menu->items.at(3)->subMenu.toggleFlags.at(1);
}

void Game::toggleOutlineBuildings() {
  menu->items.at(3)->subMenu.toggleFlags.at(2) =
      !menu->items.at(3)->subMenu.toggleFlags.at(2);
}

void Game::greenRed() {
  for (int i = 3; i < 7; i++) {
    menu->items.at(3)->subMenu.toggleFlags.at(i) = false;
  }
  menu->items.at(3)->subMenu.toggleFlags.at(3) = true;
  setColors("GREEN", "RED", 0, 255, 0, 255, 0, 0);
  if (playerSpawnID == SPAWNER_ID_ONE) {
    panel->addText("You are the GREEN team.");
  } else {
    panel->addText("You are the RED team.");
  }
}

void Game::orangeBlue() {
  for (int i = 3; i < 7; i++) {
    menu->items.at(3)->subMenu.toggleFlags.at(i) = false;
  }
  menu->items.at(3)->subMenu.toggleFlags.at(4) = true;
  setColors("ORANGE", "BLUE", 255, 128, 0, 0, 0, 255);
  if (playerSpawnID == SPAWNER_ID_ONE) {
    panel->addText("You are the ORANGE team.");
  } else {
    panel->addText("You are the BLUE team.");
  }
}

void Game::purpleYellow() {
  for (int i = 3; i < 7; i++) {
    menu->items.at(3)->subMenu.toggleFlags.at(i) = false;
  }
  menu->items.at(3)->subMenu.toggleFlags.at(5) = true;
  setColors("PURPLE", "YELLOW", 165, 0, 165, 255, 255, 0);
  if (playerSpawnID == SPAWNER_ID_ONE) {
    panel->addText("You are the PURPLE team.");
  } else {
    panel->addText("You are the YELLOW team.");
  }
}

void Game::pinkBrown() {
  for (int i = 3; i < 7; i++) {
    menu->items.at(3)->subMenu.toggleFlags.at(i) = false;
  }
  menu->items.at(3)->subMenu.toggleFlags.at(6) = true;
  setColors("PINK", "BROWN", 255, 0, 255, 123, 63, 0);
  if (playerSpawnID == SPAWNER_ID_ONE) {
    panel->addText("You are the PINK team.");
  } else {
    panel->addText("You are the BROWN team.");
  }
}

void Game::zoomIn() {
  if (scale == MAX_SCALE)
    return;
  scale += 1.0;
  adjustViewToScale();
}

void Game::zoomOut() {
  if (scale == initScale)
    return;
  scale -= 1.0;
  adjustViewToScale();
}

void Game::adjustViewToScale() {
  view.w = (int)((double)gameDisplaySize / scale);
  view.h = (int)((double)gameDisplaySize / scale);
  view.x = selectedUnit->x - view.w / 2;
  view.y = selectedUnit->y - view.h / 2;
  if (view.x < 0)
    view.x = 0;
  if (view.y < 0)
    view.y = 0;
  if (view.x > gameSize - view.w)
    view.x = gameSize - view.w;
  if (view.y > gameSize - view.h)
    view.y = gameSize - view.h;
  if (selectionContext == SELECTION_CONTEXT_UNSELECTED) {
    selectedUnit = mapUnitAt(view.x + view.w / 2, view.y + view.h / 2);
  }
  mouseMoved(mouseX, mouseY);
}

void Game::mouseMoved(int x, int y) {
  if (y >= gameDisplaySize || x >= gameDisplaySize)
    return;
  mouseX = x;
  mouseY = y;
  int mouseUnitX = (int)((double)x / scale) + view.x;
  int mouseUnitY = (int)((double)y / scale) + view.y;
  int potX = selection.x;
  int potY = selection.y;
  int potW = selection.w;
  int potH = selection.h;
  switch (selectionContext) {
  case SELECTION_CONTEXT_UNSELECTED:
    potX = mouseUnitX;
    potY = mouseUnitY;
    potW = 1;
    potH = 1;
    if (!potentialSelectionCollidesWithObjective(potX, potY, potW, potH)) {
      selectedUnit = mapUnitAt(mouseUnitX, mouseUnitY);
      selection.x = potX;
      selection.y = potY;
      selection.w = potW;
      selection.h = potH;
    }
    break;
  case SELECTION_CONTEXT_PLACING:
    potX = mouseUnitX - (placementW / 2);
    potY = mouseUnitY - (placementH / 2);
    potW = placementW;
    potH = placementH;
    if (potX + potW > gameSize - 1)
      potX = gameSize - potW - 1;
    if (potY + potH > gameSize - 1)
      potY = gameSize - potH - 1;
    if (potX < 1)
      potX = 1;
    if (potY < 1)
      potY = 1;
    if (!potentialSelectionCollidesWithObjective(potX, potY, potW, potH) &&
        !potentialSelectionCollidesWithBuilding(potX, potY, potW, potH)) {
      selection.x = potX;
      selection.y = potY;
      selection.w = potW;
      selection.h = potH;
    }
    break;
  case SELECTION_CONTEXT_SELECTING:
    if (mouseUnitX > (int)selectedUnit->x) {
      potW = mouseUnitX - selectedUnit->x + 1;
    } else {
      potX = mouseUnitX;
      potW = selectedUnit->x - mouseUnitX;
    }
    if (mouseUnitY > (int)selectedUnit->y) {
      potH = mouseUnitY - selectedUnit->y + 1;
    } else {
      potY = mouseUnitY;
      potH = selectedUnit->y - mouseUnitY;
    }
    if (potW <= 0)
      potW = 1;
    if (potH <= 0)
      potH = 1;
    if (potX + potW > gameSize)
      potW = gameSize - potX;
    if (potY + potH > gameSize)
      potH = gameSize - potY;
    if (potX < 0)
      potX = 0;
    if (potY < 0)
      potY = 0;
    if (!potentialSelectionCollidesWithObjective(potX, potY, potW, potH)) {
      selection.x = potX;
      selection.y = potY;
      selection.w = potW;
      selection.h = potH;
    }
    break;
  case SELECTION_CONTEXT_SELECTED:
    break;
  default:
    break;
  }
}

void Game::leftMouseDown(int x, int y) {
  if (x >= gameDisplaySize)
    return;
  if (y >= gameDisplaySize) {
    int idx = (x / menuSize);
    (menu->items.at(idx)->*(menu->items.at(idx)->menuFunc))();
    return;
  } else {
    mouseMoved(x, y);
    for (MenuItem *it : menu->items) {
      if (it->subMenuShown) {
        if (x > it->subMenu.x && x < it->subMenu.x + it->subMenu.w &&
            y + panelYDrawOffset > it->subMenu.y &&
            y + panelYDrawOffset < it->subMenu.y + it->subMenu.h) {
          return;
        }
      }
    }
    int mouseUnitX = (int)((double)x / scale) + view.x;
    int mouseUnitY = (int)((double)y / scale) + view.y;
    if (menu->getIfObjectivesShown()) {
      if (selectedObjective) {
        if (mouseUnitX < selectedObjective->region.x ||
            mouseUnitX >
                selectedObjective->region.x + selectedObjective->region.w - 1 ||
            mouseUnitY < selectedObjective->region.y ||
            mouseUnitY >
                selectedObjective->region.y + selectedObjective->region.h - 1) {
          selectedObjective = nullptr;
        }
      } else {
        for (Objective *o : objectives) {
          if (o->sid == playerSpawnID && mouseUnitX >= o->region.x &&
              mouseUnitX <= o->region.x + o->region.w - 1 &&
              mouseUnitY >= o->region.y &&
              mouseUnitY <= o->region.y + o->region.h - 1) {
            selectedObjective = o;
          }
        }
      }
    }
    switch (selectionContext) {
    case SELECTION_CONTEXT_PLACING:
      break;
    default:
      if (selectedObjective == nullptr) {
        selectionContext = SELECTION_CONTEXT_UNSELECTED;
        mouseMoved(x, y);
        selectionContext = SELECTION_CONTEXT_SELECTING;
      }
      break;
    }
  }
}

void Game::leftMouseUp(int x, int y) {
  if (selectionContext == SELECTION_CONTEXT_SELECTING)
    selectionContext = SELECTION_CONTEXT_SELECTED;
  if (selectionContext == SELECTION_CONTEXT_PLACING) {
    switch (placingType) {
    case BUILDING_TYPE_TOWER:
      setObjective(OBJECTIVE_TYPE_BUILD_TOWER);
      break;
    case BUILDING_TYPE_SUBSPAWNER:
      setObjective(OBJECTIVE_TYPE_BUILD_SUBSPAWNER);
      break;
    case BUILDING_TYPE_BOMB:
      setObjective(OBJECTIVE_TYPE_BUILD_BOMB);
      break;
    default:
      break;
    }
    selectionContext = SELECTION_CONTEXT_UNSELECTED;
    return;
  }
  for (MenuItem *it : menu->items) {
    if (it->subMenuShown) {
      if (x > it->subMenu.x && x < it->subMenu.x + it->subMenu.w &&
          y + panelYDrawOffset > it->subMenu.y &&
          y + panelYDrawOffset < it->subMenu.y + it->subMenu.h) {
        int idy = (y + panelYDrawOffset - it->subMenu.y) /
                  (it->subMenu.h / it->subMenu.strings.size());
        (this->*(it->subMenu.funcs.at(idy)))();
        menu->hideAllSubMenus();
        return;
      }
    }
  }
}

void Game::deselect() {
  selectionContext = SELECTION_CONTEXT_UNSELECTED;
  menu->hideAllSubMenus();
  selectedObjective = nullptr;
  resignConfirmation = false;
}

void Game::rightMouseDown(int x, int y) {
  deselect();
  mouseMoved(x, y);
}

void Game::panViewLeft() {
  view.x -= view.w / 4;
  if (view.x < 0)
    view.x = 0;
}

void Game::panViewRight() {
  view.x += view.w / 4;
  if (view.x > gameSize - view.w)
    view.x = gameSize - view.w;
}

void Game::panViewUp() {
  view.y -= view.h / 4;
  if (view.y < 0)
    view.y = 0;
}

void Game::panViewDown() {
  view.y += view.h / 4;
  if (view.y > gameSize - view.h)
    view.y = gameSize - view.h;
}

/*----------------Display Functions------------------*/

void Game::setColors(std::string p1n, std::string p2n, int p1r, int p1g,
                     int p1b, int p2r, int p2g, int p2b) {
  colorScheme.p1name = p1n;
  colorScheme.p2name = p2n;
  colorScheme.p1r = p1r;
  colorScheme.p1g = p1g;
  colorScheme.p1b = p1b;
  colorScheme.p2r = p2r;
  colorScheme.p2g = p2g;
  colorScheme.p2b = p2b;
  if (p1bombTexture != nullptr) {
    SDL_DestroyTexture(p1bombTexture);
    SDL_DestroyTexture(p2bombTexture);
  }
  p1bombTexture = disp->cacheImageColored("assets/img/bomb.png", p1r, p1g, p1b);
  p2bombTexture = disp->cacheImageColored("assets/img/bomb.png", p2r, p2g, p2b);
}

void Game::setTeamDrawColor(SpawnerID sid) {
  switch (sid) {
  case SPAWNER_ID_ONE:
    disp->setDrawColor(colorScheme.p1r, colorScheme.p1g, colorScheme.p1b);
    break;
  case SPAWNER_ID_TWO:
    disp->setDrawColor(colorScheme.p2r, colorScheme.p2g, colorScheme.p2b);
    break;
  }
}

void Game::drawStartupScreen() {
  disp->fillBlack();
  std::string startupInfoL1 = "You are the";
  std::string startupInfoL2;
  std::string startupInfoL3 = "team";
  std::string startupInfoL4 =
      std::to_string(secondsRemaining - GAME_TIME_SECONDS);
  int r, g;
  int b = 0;
  switch (playerSpawnID) {
  case SPAWNER_ID_ONE:
    startupInfoL2 = colorScheme.p1name;
    r = colorScheme.p1r;
    g = colorScheme.p1g;
    b = colorScheme.p1b;
    break;
  case SPAWNER_ID_TWO:
    startupInfoL2 = colorScheme.p2name;
    r = colorScheme.p2r;
    g = colorScheme.p2g;
    b = colorScheme.p2b;
    break;
  }
  int w1, h1, w2, h2, w3, h3, w4, h4;
  disp->sizeTextSized(startupInfoL1.c_str(), STARTUP_FONT_SIZE, &w1, &h1);
  disp->sizeTextSized(startupInfoL2.c_str(), STARTUP_FONT_SIZE, &w2, &h2);
  disp->sizeTextSized(startupInfoL3.c_str(), STARTUP_FONT_SIZE, &w3, &h3);
  disp->sizeTextSized(startupInfoL4.c_str(), STARTUP_FONT_SIZE, &w4, &h4);
  int xOff = disp->getWidth() / 2 - w1 / 2;
  int yOff = disp->getHeight() / 3;
  disp->drawTextSizedColored(startupInfoL1.c_str(), xOff, yOff,
                             STARTUP_FONT_SIZE, 255, 255, 255);
  xOff = disp->getWidth() / 2 - w2 / 2;
  yOff += h1;
  disp->drawTextSizedColored(startupInfoL2.c_str(), xOff, yOff,
                             STARTUP_FONT_SIZE, r, g, b);
  xOff = disp->getWidth() / 2 - w3 / 2;
  yOff += h2;
  disp->drawTextSizedColored(startupInfoL3.c_str(), xOff, yOff,
                             STARTUP_FONT_SIZE, 255, 255, 255);
  xOff = disp->getWidth() / 2 - w4 / 2;
  yOff += h3;
  disp->drawTextSizedColored(startupInfoL4.c_str(), xOff, yOff,
                             STARTUP_FONT_SIZE, 255, 255, 255);
}

void Game::drawEffects() {
  for (auto it = towerZaps.begin(); it != towerZaps.end(); it++) {
    disp->setDrawColorWhite();
    int x1 = scaleInt(it->x1 - view.x);
    int y1 = scaleInt(it->y1 - view.y) + panelYDrawOffset;
    int x2 = scaleInt(it->x2 - view.x);
    int y2 = scaleInt(it->y2 - view.y) + panelYDrawOffset;
    SDL_Point effectPoints[ZAP_EFFECTS_SUBDIVISION];
    for (int i = 0; i < ZAP_EFFECTS_SUBDIVISION; i++) {
      double t = (double)i / (double)(ZAP_EFFECTS_SUBDIVISION - 1);
      int tx = (int)((1.0 - t) * (double)x1 + t * (double)x2);
      int ty = (int)((1.0 - t) * (double)y1 + t * (double)y2);
      if (i != 0 && i != ZAP_EFFECTS_SUBDIVISION - 1) {
        tx += (rand() % 2 * (int)scale) - (int)scale;
        ty += (rand() % 2 * (int)scale) - (int)scale;
      }
      effectPoints[i] = {tx, ty};
    }
    disp->drawLines(effectPoints, ZAP_EFFECTS_SUBDIVISION);
  }
  for (auto it = bombEffects.begin(); it != bombEffects.end(); it++) {
    int x = scaleInt(it->x - view.x);
    int y = scaleInt(it->y - view.y) + panelYDrawOffset;
    int maxRad = scaleInt(BOMB_AOE_RADIUS);
    int r = rand() % 255;
    int g = rand() % 255;
    int b = rand() % 255;
    disp->setDrawColor(r, g, b);
    for (int i = 1; i <= maxRad; i++) {
      disp->drawCircle(x, y, i);
    }
  }
}

void Game::drawBuilding(Building *build) {
  int x, y, s;
  Tower *t;
  Bomb *b;
  SDL_Point centerPoints[ZAP_CENTER_EFFECTS_NUM];
  SDL_Texture *texture;
  switch (build->type) {
  case BUILDING_TYPE_SUBSPAWNER:
  case BUILDING_TYPE_SPAWNER:
    break;
  case BUILDING_TYPE_TOWER:
    t = (Tower *)build;
    setTeamDrawColor(t->sid);
    x = scaleInt(t->region.x - view.x);
    y = scaleInt(t->region.y - view.y) + panelYDrawOffset;
    s = scaleInt(TOWER_SIZE);
    disp->drawRectFilled(x, y + s / 4, s, s / 2);
    disp->drawRectFilled(x + s / 4, y, s / 2, s);
    disp->drawLine(x, y + s / 4, x + s / 4, y);
    disp->drawLine(x + s - s / 4, y, x + s, y + s / 4);
    disp->drawLine(x + s, y + s - s / 4, x + s - s / 4, y + s);
    disp->drawLine(x + s / 4, y + s, x, y + s - s / 4);
    disp->setDrawColorBlack();
    for (int i = 1; i < s / 4; i++) {
      disp->drawCircle(x + s / 2, y + s / 2, i);
    }
    for (int i = 0; i < ZAP_CENTER_EFFECTS_NUM; i++) {
      int tx = x + s / 2 + (rand() % (s / 3)) - s / 6;
      int ty = y + s / 2 + (rand() % (s / 3)) - s / 6;
      centerPoints[i] = {tx, ty};
    }
    disp->setDrawColorWhite();
    if (t->hp < t->max_hp) {
      disp->setDrawColorBrightness((double)t->hp / (double)t->max_hp);
    }
    disp->drawLines(centerPoints, ZAP_CENTER_EFFECTS_NUM);
    break;
  case BUILDING_TYPE_BOMB:
    b = (Bomb *)build;
    switch (b->sid) {
    case SPAWNER_ID_ONE:
      texture = p1bombTexture;
      break;
    case SPAWNER_ID_TWO:
      texture = p2bombTexture;
      break;
    }
    double completion = (double)b->hp / (double)b->max_hp;
    x = scaleInt(b->region.x - view.x);
    y = scaleInt(b->region.y - view.y) + panelYDrawOffset;
    s = scaleInt(BOMB_SIZE);
    int rectHeight = (int)((double)s * completion);
    disp->setDrawColorWhite();
    disp->drawRectFilled(x, y + s - rectHeight, s, rectHeight);
    disp->drawTexture(texture, x, y, s, s);
    break;
  }
}

void Game::draw() {
  disp->fillBlack();
  disp->setDrawColorBlack();
  disp->drawRectFilled(0, panelYDrawOffset, gameDisplaySize, gameDisplaySize);
  int lum;
  double lumprop;
  MapUnit *first = mapUnitAt(view.x, view.y);
  for (MapUnit::iterator iter = first->getIterator(view.w, view.h);
       iter.hasNext(); iter++) {
    int scaledX = scaleInt(iter->x - view.x);
    int scaledY = scaleInt(iter->y - view.y) + panelYDrawOffset;
    int off;
    double offProp;
    switch (iter->type) {
    case UNIT_TYPE_AGENT:
      setTeamDrawColor(iter->agent->sid);
      disp->drawRectFilled(scaledX, scaledY, (int)scale, (int)scale);
      break;
    case UNIT_TYPE_BUILDING:
      break;
    case UNIT_TYPE_DOOR:
      setTeamDrawColor(iter->door->sid);
      disp->drawRectFilled(scaledX, scaledY, (int)scale, (int)scale);
      if (iter->door->hp < MAX_DOOR_HEALTH || iter->door->isEmpty) {
        if (menu->getIfScentsShown()) {
          lum = (int)(255.0 * (double)iter->playerDict[playerSpawnID].scent /
                      255.0);
          disp->setDrawColor(lum, 0, lum);
        } else {
          disp->setDrawColorBlack();
        }
        offProp = (double)iter->door->hp / (double)MAX_DOOR_HEALTH;
        off = (int)(offProp * scale * 1.0 / 4.0);
        disp->drawRectFilled(scaledX + (int)(scale / 2.0) - off,
                             scaledY + (int)(scale / 2.0) - off, 2 * off,
                             2 * off);
      }
      break;
    case UNIT_TYPE_SPAWNER:
      setTeamDrawColor(iter->building->sid);
      lumprop = (double)iter->hp / (double)SUBSPAWNER_UNIT_COST;
      disp->setDrawColorBrightness(lumprop);
      if (((iter->x + iter->y) % 2) == 0)
        disp->setDrawColorBrightness(0.5);
      disp->drawRectFilled(scaledX, scaledY, (int)scale, (int)scale);
      break;
    case UNIT_TYPE_WALL:
      lum = (int)(((double)iter->hp / (double)MAX_WALL_HEALTH) * 100.0) + 155;
      disp->setDrawColor(lum, lum, lum);
      disp->drawRectFilled(scaledX, scaledY, (int)scale, (int)scale);
      break;
    case UNIT_TYPE_EMPTY:
      if (menu->getIfScentsShown()) {
        lum = (int)(255.0 * (double)iter->playerDict[playerSpawnID].scent /
                    255.0);
        disp->setDrawColor(lum, 0, lum);
      } else {
        disp->setDrawColorBlack();
      }
      disp->drawRectFilled(scaledX, scaledY, (int)scale, (int)scale);
      break;
    default:
      break;
    }
  }
  drawEffects();
  for (auto it = buildingLists.begin(); it != buildingLists.end(); it++) {
    for (Building *build : it->second) {
      drawBuilding(build);
    }
  }
  if (selectionContext != SELECTION_CONTEXT_UNSELECTED) {
    disp->setDrawColor(150, 150, 150);
    int selectionScaledX = scaleInt(selection.x - view.x);
    int selectionScaledY = scaleInt(selection.y - view.y) + panelYDrawOffset;
    disp->drawRect(selectionScaledX, selectionScaledY, scaleInt(selection.w),
                   scaleInt(selection.h));
  }
  if (selectionContext == SELECTION_CONTEXT_PLACING) {
    ObjectiveType oType;
    switch (placingType) {
    case BUILDING_TYPE_TOWER:
      oType = OBJECTIVE_TYPE_BUILD_TOWER;
      break;
    case BUILDING_TYPE_SUBSPAWNER:
      oType = OBJECTIVE_TYPE_BUILD_SUBSPAWNER;
      break;
    case BUILDING_TYPE_BOMB:
      oType = OBJECTIVE_TYPE_BUILD_BOMB;
      break;
    default:
      oType = OBJECTIVE_TYPE_BUILD_TOWER;
      break;
    }
    SDL_Texture *texture = objectiveInfoTextures[oType];
    int objectiveInfoWidth;
    int objectiveInfoHeight;
    SDL_QueryTexture(texture, NULL, NULL, &objectiveInfoWidth,
                     &objectiveInfoHeight);
    int x = 0;
    int y = gameDisplaySize + panelYDrawOffset - objectiveInfoHeight;
    disp->drawTexture(texture, x, y);
  }
  if (menu->getIfBuildingsOutlined()) {
    disp->setDrawColorWhite();
    for (auto it = buildingLists.begin(); it != buildingLists.end(); it++) {
      for (Building *build : it->second) {
        disp->drawRect(scaleInt(build->region.x - view.x),
                       scaleInt(build->region.y - view.y) + panelYDrawOffset,
                       scaleInt(build->region.w), scaleInt(build->region.h));
      }
    }
  }
  if (menu->getIfObjectivesShown()) {
    for (Objective *o : objectives) {
      if (!(o->sid == playerSpawnID)) {
        continue;
      }
      disp->setDrawColor(255, 255, 0);
      if (o == selectedObjective) {
        disp->setDrawColor(0, 255, 255);
      }
      int scaledX = scaleInt(o->region.x - view.x);
      int scaledY = scaleInt(o->region.y - view.y) + panelYDrawOffset;
      int scaledW = scaleInt(o->region.w);
      int scaledH = scaleInt(o->region.h);
      disp->drawRect(scaledX, scaledY, scaledW, scaledH);
      disp->drawRect(scaledX - 1, scaledY - 1, scaledW + 2, scaledH + 2);
    }
    if (selectedObjective) {
      SDL_Texture *texture = objectiveInfoTextures[selectedObjective->type];
      int objectiveInfoWidth;
      int objectiveInfoHeight;
      SDL_QueryTexture(texture, NULL, NULL, &objectiveInfoWidth,
                       &objectiveInfoHeight);
      int x = 0;
      int y = gameDisplaySize + panelYDrawOffset - objectiveInfoHeight;
      disp->drawTexture(texture, x, y);
    }
  }
  disp->setDrawColorWhite();
  disp->drawRect(0, panelYDrawOffset, gameDisplaySize, gameDisplaySize);
  int m = secondsRemaining / 60;
  int s = secondsRemaining % 60;
  std::string mText = std::to_string(m);
  if (m < 10)
    mText = "0" + mText;
  std::string sText = std::to_string(s);
  if (s < 10)
    sText = "0" + sText;
  std::string timeText = mText + ":" + sText;
  int ttw, tth;
  disp->sizeText(timeText.c_str(), &ttw, &tth);
  if (gameMode == 0) {
    disp->drawText(timeText.c_str(), gameDisplaySize - ttw, panelYDrawOffset);
  }
  panel->flushText();
  panel->draw();
  menu->draw(disp);
}

/*-------------------Helper functions &
 * one-liners------------------------------*/

bool Game::rectCollides(SDL_Rect r1, SDL_Rect r2) {
  return (r1.x < r2.x + r2.w && r1.x + r1.w > r2.x && r1.y < r2.y + r2.h &&
          r1.y + r1.h > r2.y);
}

bool Game::potentialSelectionCollidesWithBuilding(int potX, int potY, int potW,
                                                  int potH) {
  SDL_Rect r = {potX, potY, potW, potH};
  for (auto it = buildingLists.begin(); it != buildingLists.end(); it++) {
    for (Building *build : it->second) {
      if (rectCollides(r, build->region))
        return true;
    }
  }
  return false;
}

bool Game::potentialSelectionCollidesWithObjective(int potX, int potY, int potW,
                                                   int potH) {
  SDL_Rect r = {potX, potY, potW, potH};
  for (Objective *o : objectives) {
    if (o->sid == playerSpawnID && rectCollides(r, o->region))
      return true;
  }
  return false;
}

int Game::scaleInt(int toScale) { return (int)(scale * ((double)toScale)); }
int Game::getSize() { return gameSize; }
Context Game::getContext() { return context; }
SpawnerID Game::getPlayerSpawnID() { return playerSpawnID; }
AgentID Game::getNewAgentID() { return newAgentID++; }
void Game::buildWall() { setObjective(OBJECTIVE_TYPE_BUILD_WALL); }
void Game::goTo() { setObjective(OBJECTIVE_TYPE_GOTO); }
void Game::buildDoor() { setObjective(OBJECTIVE_TYPE_BUILD_DOOR); }
void Game::attack() { setObjective(OBJECTIVE_TYPE_ATTACK); }
void Game::placeTower() { placeBuilding(BUILDING_TYPE_TOWER); }
void Game::placeSubspawner() { placeBuilding(BUILDING_TYPE_SUBSPAWNER); }
void Game::placeBomb() { placeBuilding(BUILDING_TYPE_BOMB); }
void Game::showControls() { panel->controlsText(); }
void Game::showBasicInfo() { panel->basicInfoText(); }
void Game::showCosts() { panel->costsText(); }
void Game::clearPanel() { panel->clearText(); }
void Game::markAgentForDeletion(AgentID id) { markedAgents.push_back(id); }
void Game::markBuildingForDeletion(Building *build) { markedBuildings.push_back(build); }
MapUnit *Game::mapUnitAt(int x, int y) { return mapUnits[y * gameSize + x]; }
MapUnit::iterator Game::getSelectionIterator() {
  return mapUnitAt(selection.x + view.x, selection.y + view.y)
      ->getIterator(selection.w, selection.h);
}

/*----------------------Main event loop------------------------*/

void Game::confirmResign() {
  if (!resignConfirmation) {
    panel->addText("Are you sure you want to resign? Select 'Resign' again to "
                   "confirm or use the X to cancel.");
    resignConfirmation = true;
  } else {
    resign();
  }
}

void Game::resign() {
  switch (playerSpawnID) {
  case SPAWNER_ID_ONE:
    winnerSpawnID = SPAWNER_ID_TWO;
    break;
  case SPAWNER_ID_TWO:
    winnerSpawnID = SPAWNER_ID_ONE;
    break;
  }
  end(DONE_STATUS_RESIGN);
}

void Game::standardizeEventCoords(float x, float y, int *retx, int *rety) {
  *retx = (int)(x * (float)disp->getWidth());
  *rety = (int)(y * (float)disp->getHeight()) - panelSize;
}

void Game::handleSDLEventMobile(SDL_Event *e) {
  if (e->type == SDL_QUIT) {
    context = GAME_CONTEXT_EXIT;
    return;
  }
  if (context != GAME_CONTEXT_PLAYING && context != GAME_CONTEXT_DONE && context != GAME_CONTEXT_PRACTICE)
    return;
  int x, y;
  switch (e->type) {
  case SDL_FINGERDOWN:
    standardizeEventCoords(e->tfinger.x, e->tfinger.y, &x, &y);
    if (y <= 0) {
      deselect();
      break;
    };
    leftMouseDown(x, y);
    break;
  case SDL_FINGERUP:
    standardizeEventCoords(e->tfinger.x, e->tfinger.y, &x, &y);
    if (y <= 0)
      break;
    leftMouseUp(x, y);
    break;
  case SDL_FINGERMOTION:
    standardizeEventCoords(e->tfinger.x, e->tfinger.y, &x, &y);
    if (y > 0) {
      mouseMoved(x, y);
      break;
    }
    if (e->tfinger.dy < 0)
      panel->scrollUp();
    if (e->tfinger.dy > 0)
      panel->scrollDown();
    break;
  case SDL_MULTIGESTURE:
    standardizeEventCoords(e->mgesture.x, e->mgesture.y, &x, &y);
    if (e->mgesture.numFingers == 1) {
      if (y > 0)
        mouseMoved(x, y);
      break;
    }
    if (e->mgesture.dDist < 0.01 && e->mgesture.dDist > -0.01)
      deselect();
    else {
      if (e->mgesture.dDist > 0)
        zoomIn();
      else
        zoomOut();
    }
    break;
  default:
    break;
  }
}

void Game::handleSDLEvent(SDL_Event *e) {
  int x, y;
  if (e->type == SDL_QUIT) {
    context = GAME_CONTEXT_EXIT;
    return;
  }
  if (context != GAME_CONTEXT_PLAYING && context != GAME_CONTEXT_DONE && context != GAME_CONTEXT_PRACTICE)
    return;
  SDL_GetMouseState(&x, &y);
  switch (e->type) {
  case SDL_MOUSEWHEEL:
    if (x >= gameDisplaySize) {
      if (e->wheel.y > 0)
        panel->scrollDown();
      else if (e->wheel.y < 0)
        panel->scrollUp();
      break;
    }
    if (y >= gameDisplaySize)
      break;
    if (e->wheel.y > 0)
      zoomIn();
    else if (e->wheel.y < 0)
      zoomOut();
    break;
  case SDL_MOUSEMOTION:
    mouseMoved(x, y);
    break;
  case SDL_MOUSEBUTTONDOWN:
    switch (e->button.button) {
    case SDL_BUTTON_LEFT:
      leftMouseDown(x, y);
      break;
    case SDL_BUTTON_RIGHT:
      rightMouseDown(x, y);
      break;
    }
    mouseMoved(x, y);
    break;
  case SDL_MOUSEBUTTONUP:
    switch (e->button.button) {
    case SDL_BUTTON_LEFT:
      leftMouseUp(x, y);
      break;
    }
    mouseMoved(x, y);
    break;
  case SDL_KEYDOWN:
    switch (e->key.keysym.sym) {
    case SDLK_ESCAPE:
      deselect();
    case SDLK_SPACE:
      break;
    case SDLK_UP:
      panViewUp();
      mouseMoved(x, y);
      break;
    case SDLK_DOWN:
      panViewDown();
      mouseMoved(x, y);
      break;
    case SDLK_RIGHT:
      panViewRight();
      mouseMoved(x, y);
      break;
    case SDLK_LEFT:
      panViewLeft();
      mouseMoved(x, y);
      break;
    case SDLK_w:
      buildWall();
      break;
    case SDLK_d:
      buildDoor();
      break;
    case SDLK_c:
      clearScent();
      break;
    case SDLK_a:
      attack();
      break;
    case SDLK_g:
      goTo();
      break;
    case SDLK_t:
      placeTower();
      break;
    case SDLK_b:
      placeBomb();
      break;
    case SDLK_s:
      placeSubspawner();
      break;
    case SDLK_BACKSPACE:
    case SDLK_DELETE:
      deleteSelectedObjective();
      break;
    }
    break;
  }
}

void Game::mainLoop(void) {
  pthread_mutex_lock(&threadLock);
  switch (context) {
  case GAME_CONTEXT_CONNECTING:
    disp->fillBlack();
    disp->drawText("Connecting...", 0, 0);
    if (net->ncon == NET_CONTEXT_CONNECTED || net->ncon == NET_CONTEXT_READY ||
        net->ncon == NET_CONTEXT_CLOSED) {
      disp->drawText("Waiting for opponent...", 0, 40);
    }
    if (net->ncon == NET_CONTEXT_CLOSED) {
      disp->drawText("Connection closed, likely bad token", 0, 80);
    }
    break;
  case GAME_CONTEXT_STARTUPTIMER:
    drawStartupScreen();
    break;
  case GAME_CONTEXT_PRACTICE:
    playerSpawnID = SPAWNER_ID_TWO;
    update();
    receiveEventsBuffer();
    playerSpawnID = SPAWNER_ID_ONE;
    update();
    receiveEventsBuffer();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  default:
    draw();
    break;
  }
  auto tzit = towerZaps.begin();
  while (tzit != towerZaps.end()) {
    if (std::chrono::high_resolution_clock::now() - tzit->start >= std::chrono::milliseconds(ZAP_CLEAR_TIME)) {
      tzit = towerZaps.erase(tzit);
    } else tzit++;
  }
  auto beit = bombEffects.begin();
  while (beit != bombEffects.end()) {
    if (std::chrono::high_resolution_clock::now() - beit->start >= std::chrono::milliseconds(BOMB_CLEAR_TIME)) {
      beit = bombEffects.erase(beit);
    } else beit++;
  }
  SDL_PumpEvents();
  SDL_Event e;
  while (SDL_PeepEvents(&e, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) >
         0) {
    if (mobile) {
      handleSDLEventMobile(&e);
    } else {
      handleSDLEvent(&e);
    }
  }
  disp->update();
  pthread_mutex_unlock(&threadLock);
}
