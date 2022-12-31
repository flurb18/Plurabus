#include "game.h"

#include <emscripten.h>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_rect.h>
#include <iostream>
#include <cstdlib>
#include <stdlib.h>
#include <string>
#include <random>

#include "agent.h"
#include "constants.h"
#include "display.h"
#include "door.h"
#include "event.h"
#include "menu.h"
#include "nethandler.h"
#include "panel.h"
#include "spawner.h"

/* Constructor sets up map units, creates a friendly spawner */
Game::Game(int sz, int psz, int numMenIt, double scl, int ulim, char *pstr):
  
  numPlayerAgents(0),
  numPlayerTowers(0),
  context(GAME_CONTEXT_CONNECTING),
  initScale(scl),
  scale(scl),
  gameSize(sz),
  menuItemsInView(numMenIt),
  panelSize(psz),
  unitLimit(ulim),
  mouseX(0),
  mouseY(0),
  placementW(0),
  placementH(0),
  zapCounter(1),
  newAgentID(1),
  newTowerID(1),
  outside(this),
  selectedObjective(nullptr) {

  net = new NetHandler(this, pstr);
  gameDisplaySize = scaleInt(gameSize);
  /* mapUnits is a vector */
  mapUnits.reserve(gameSize * gameSize);
  /* Units are created in the x direction; it fills the top row with map units
  from left to right, then the next row and so on
  SDL window x and y start at 0 at top left and increase right and
  down respectively*/
  for (unsigned int i = 0; i < gameSize; i++) {
    for (unsigned int j = 0; j < gameSize; j++) {
      mapUnits.push_back(new MapUnit(this, j, i));
    }
  }
  /* Interlink the map units so they all know their adjacent units */
  for (unsigned int i = 0; i < gameSize; i++) {
    for (unsigned int j = 0; j < gameSize; j++) {
      int index = i * gameSize + j;
      if (i == 0) mapUnits[index]->up = &outside;
      else mapUnits[index]->up = mapUnits[index - gameSize];
      if (i == gameSize - 1) mapUnits[index]->down = &outside;
      else mapUnits[index]->down = mapUnits[index + gameSize];
      if (j == 0) mapUnits[index]->left = &outside;
      else mapUnits[index]->left = mapUnits[index - 1];
      if (j == gameSize - 1) mapUnits[index]->right = &outside;
      else mapUnits[index]->right = mapUnits[index + 1];
    }
  }

  selection = {0, 0, 1, 1};
  menuSize = gameDisplaySize / menuItemsInView;
  disp = new Display(gameDisplaySize, menuSize, panelSize, FONT_SIZE);
  panel = new Panel(disp, PANEL_PADDING);
  std::string welcomeText = "Welcome to " + std::string(TITLE) + "!";
  panel->addText(welcomeText.c_str());
  view = {0,0,gameSize,gameSize};
  selectedUnit = mapUnitAt(0,0);
  menu = new Menu(this, menuItemsInView);
  MapUnit *redspawn = mapUnitAt(SPAWNER_PADDING, SPAWNER_PADDING);
  MapUnit *greenspawn = mapUnitAt(gameSize - SPAWNER_SIZE - SPAWNER_PADDING, gameSize - SPAWNER_SIZE - SPAWNER_PADDING);
  spawnerDict.insert(std::make_pair(SPAWNER_ID_GREEN, new Spawner(this, greenspawn, SPAWNER_ID_GREEN, SPAWNER_SIZE, 1)));
  spawnerDict.insert(std::make_pair(SPAWNER_ID_RED, new Spawner(this, redspawn, SPAWNER_ID_RED, SPAWNER_SIZE, 1)));
  objectiveInfoTextures[OBJECTIVE_TYPE_BUILD_WALL] = disp->cacheTextWrapped("Objective - Build Wall", 0);
  objectiveInfoTextures[OBJECTIVE_TYPE_ATTACK] = disp->cacheTextWrapped("Objective - Attack", 0);
  objectiveInfoTextures[OBJECTIVE_TYPE_GOTO] = disp->cacheTextWrapped("Objective - Go To", 0);
  objectiveInfoTextures[OBJECTIVE_TYPE_BUILD_DOOR] = disp->cacheTextWrapped("Objective - Build Door", 0);
  objectiveInfoTextures[OBJECTIVE_TYPE_BUILD_TOWER] = disp->cacheTextWrapped("Objective - Build Tower", 0);
  objectiveInfoTextures[OBJECTIVE_TYPE_BUILD_SUBSPAWNER] = disp->cacheTextWrapped("Objective - Build Subspawner", 0);
}

/* Public helper method */
MapUnit* Game::mapUnitAt(int x, int y) {
  return mapUnits[y * gameSize + x];
}

void Game::zoomIn() {
  if (scale == MAX_SCALE) return;
  scale += 1.0;
  adjustViewToScale();
}

void Game::zoomOut() {
  if (scale == initScale) return;
  scale -= 1.0;
  adjustViewToScale();
}

void Game::adjustViewToScale() {
  view.w = (int)((double)gameDisplaySize / scale);
  view.h = (int)((double)gameDisplaySize / scale);
  view.x = selectedUnit->x - view.w/2;
  view.y = selectedUnit->y - view.h/2;
  if (view.x < 0) view.x = 0;
  if (view.y < 0) view.y = 0;
  if (view.x > gameSize - view.w) view.x = gameSize - view.w;
  if (view.y > gameSize - view.h) view.y = gameSize - view.h;
  if (context == GAME_CONTEXT_UNSELECTED) {
    selectedUnit = mapUnitAt(view.x + view.w/2, view.y + view.h/2);
  }
  mouseMoved(mouseX, mouseY);
}

int Game::scaleInt(int toScale) {
  return (int)(scale * ((double)toScale));
}

Context Game::getContext() {
  return context;
}

int Game::getUnitLimit() {
  return unitLimit;
}

int Game::getSize() {
  return gameSize;
}

SpawnerID Game::getPlayerSpawnID() {
  return playerSpawnID;
}

bool Game::rectCollidesOneWay(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) {
  if (x1 >= x2 && x1 <= x2 + w2 - 1) {
    if (y1 >= y2 && y1 <= y2 + h2 - 1) return true;
    if (y1 + h1 - 1 >= y2 && y1 + h1 - 1 <= y2 + h2 - 1) return true;
  }
  if (x1 + w1 - 1 >= x2 && x1 + w1 - 1 <= x2 + w2 - 1) {
    if (y1 >= y2 && y1 <= y2 + h2 - 1) return true;
    if (y1 + h1 - 1 >= y2 && y1 + h1 - 1 <= y2 + h2 - 1) return true;
  }
  return false;
}

bool Game::rectCollides(SDL_Rect r1, SDL_Rect r2) {
  return (rectCollidesOneWay(r1.x, r1.y, r1.w, r1.h, r2.x, r2.y, r2.w, r2.h) ||
	  rectCollidesOneWay(r2.x, r2.y, r2.w, r2.h, r1.x, r1.y, r1.w, r1.h));
}

bool Game::potentialSelectionCollidesWithObjective(int potX, int potY, int potW, int potH) {
  SDL_Rect r = {potX, potY, potW, potH};
  for (Objective *o: objectives) {
    if (rectCollides(r, o->region)) return true;
  }
  return false;
}

bool Game::potentialSelectionCollidesWithTower(int potX, int potY, int potW, int potH) {
  SDL_Rect r= {potX, potY, potW, potH};
  for (auto it = towerDict.begin(); it != towerDict.end(); it++) {
    Tower *t = it->second;
    if (rectCollides(r, t->region)) return true;
  }
  return false;
}

bool Game::potentialSelectionCollidesWithSpawner(int potX, int potY, int potW, int potH) {
  SDL_Rect r = {potX, potY, potW, potH};
  for (auto it = spawnerDict.begin(); it != spawnerDict.end(); it++) {
    Spawner *s = it->second;
    SDL_Rect rs = {(int)s->topLeft->x, (int)s->topLeft->y, s->size, s->size};
    if (rectCollides(r, rs)) return true;
  }
  for (Subspawner *s: subspawnerList) {
    if (rectCollides(r, s->region)) return true;
  }
  return false;
}

/* Handle a mouse moved to (x,y) */
void Game::mouseMoved(int x, int y) {
  if (y >= gameDisplaySize || x >= gameDisplaySize) {
    selectedObjective = nullptr;
    switch (context) {
    case GAME_CONTEXT_SELECTING:
      context = GAME_CONTEXT_SELECTED;
    default:
      break;
    }
    return;
  }
  mouseX = x;
  mouseY = y;
  int mouseUnitX = (int)((double)x / scale) + view.x;
  int mouseUnitY = (int)((double)y / scale) + view.y;
  if (menu->getIfObjectivesShown()) {
    if (selectedObjective) {
      if (mouseUnitX < selectedObjective->region.x ||
	  mouseUnitX > selectedObjective->region.x + selectedObjective->region.w - 1 ||
	  mouseUnitY < selectedObjective->region.y ||
	  mouseUnitY > selectedObjective->region.y + selectedObjective->region.h - 1) {
	selectedObjective = nullptr;
      }
    } else {
      for (Objective *o: objectives) {
	if (mouseUnitX >= o->region.x &&
	    mouseUnitX <= o->region.x + o->region.w - 1 &&
	    mouseUnitY >= o->region.y &&
	    mouseUnitY <= o->region.y + o->region.h - 1) {
	  selectedObjective = o;
	}
      }
    }
  }
  int potX = selection.x;
  int potY = selection.y;
  int potW = selection.w;
  int potH = selection.h;
  switch(context) {
  case GAME_CONTEXT_UNSELECTED:
    selectedUnit = mapUnitAt(mouseUnitX, mouseUnitY);
    selection.x = mouseUnitX;
    selection.y = mouseUnitY;
    selection.w = 1;
    selection.h = 1;
    break;
  case GAME_CONTEXT_PLACING:
    potX = mouseUnitX - (placementW/2);
    potY = mouseUnitY - (placementH/2);
    potW = placementW;
    potH = placementH;
    if (potX + potW > gameSize) potX = gameSize - potW;
    if (potY + potH > gameSize) potY = gameSize - potH;
    if (potX < 0) potX = 0;
    if (potY < 0) potY = 0;
    if (!potentialSelectionCollidesWithObjective(potX, potY, potW, potH) &&
	!potentialSelectionCollidesWithTower(potX, potY, potW, potH) &&
	!potentialSelectionCollidesWithSpawner(potX, potY, potW, potH)) {
      selection.x = potX;
      selection.y = potY;
      selection.w = potW;
      selection.h = potH;
    }
    break;
  case GAME_CONTEXT_SELECTING:
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
    if (potW <= 0) potW = 1;
    if (potH <= 0) potH = 1;
    if (potX + potW > gameSize) potW = gameSize - potX;
    if (potY + potH > gameSize) potH = gameSize - potY;
    if (potX < 0) potX = 0;
    if (potY < 0) potY = 0;
    if (potentialSelectionCollidesWithObjective(potX, potY, potW, potH)) {
      context = GAME_CONTEXT_SELECTED;
      break;
    }
    selection.x = potX;
    selection.y = potY;
    selection.w = potW;
    selection.h = potH;
    break;
  case GAME_CONTEXT_SELECTED:
    break;
  case GAME_CONTEXT_CONNECTING:
    break;
  default:
    break;
  }
}

/* Handle a left mouse down at (x,y) */
void Game::leftMouseDown(int x, int y) {
  if (context == GAME_CONTEXT_CONNECTING) return;
  if (x >= gameDisplaySize) return;
  if (y >= gameDisplaySize) {
    int idx = (x / menuSize) - menu->indexOffset;
    (menu->items.at(idx)->*(menu->items.at(idx)->menuFunc))();
    return;
  } else {
    for (MenuItem *it: menu->items) {
      if (it->subMenuShown) {
	if (x > it->subMenu.x &&
	    x < it->subMenu.x + it->subMenu.w &&
	    y > it->subMenu.y &&
	    y < it->subMenu.y + it->subMenu.h) {
	  int idy = (y - it->subMenu.y)/(it->subMenu.h / it->subMenu.strings.size());
	  if (it->subMenu.isToggleSubMenu) {
	    it->subMenu.toggleFlags.at(idy) = !it->subMenu.toggleFlags.at(idy);
	  } else {
	    (this->*(it->subMenu.funcs.at(idy)))();
	    menu->hideAllSubMenus();
	  }
	  return;
	}
      }
    }   
    switch(context) {
    case GAME_CONTEXT_CONNECTING:
      break;
    case GAME_CONTEXT_PLACING:
      switch (placingType) {
      case BUILDING_TYPE_TOWER:
	setObjective(OBJECTIVE_TYPE_BUILD_TOWER);
	break;
      case BUILDING_TYPE_SUBSPAWNER:
	setObjective(OBJECTIVE_TYPE_BUILD_SUBSPAWNER);
	break;
      }
    default:
      mouseMoved(x,y);
      if (selectedObjective == nullptr) {
	context = GAME_CONTEXT_UNSELECTED;
	mouseMoved(x,y);
	context = GAME_CONTEXT_SELECTING;
      }
      break;
    }
  }
}

/* Handle a left mouse up at (x,y) */
void Game::leftMouseUp(int x, int y) {
  switch(context) {
  case GAME_CONTEXT_CONNECTING:
    break;
  default:
    if (context == GAME_CONTEXT_SELECTING) context = GAME_CONTEXT_SELECTED;
    break;
  }
}

/* Handle a right mouse click at (x,y) */
void Game::rightMouseDown(int x, int y) {
  switch(context) {
  case GAME_CONTEXT_CONNECTING:
    break;
  default:
    context = GAME_CONTEXT_UNSELECTED;
    menu->hideAllSubMenus();
    break;
  }
}

void Game::panViewLeft() {
  view.x -= view.w/4;
  if (view.x < 0) view.x = 0;
}

void Game::panViewRight() {
  view.x += view.w/4;
  if (view.x > gameSize - view.w) view.x = gameSize - view.w;
}

void Game::panViewUp() {
  view.y -= view.h/4;
  if (view.y < 0) view.y = 0;
}

void Game::panViewDown() {
  view.y += view.h/4;
  if (view.y > gameSize - view.h) view.y = gameSize - view.h;
}

/* Get MapUnit iterator of the MapUnits in the current selected region */
MapUnit::iterator Game::getSelectionIterator() {
  MapUnit* first = mapUnitAt(selection.x + view.x, selection.y + view.y);
  return first->getIterator(selection.w, selection.h);
}

void Game::buildWall() {
  setObjective(OBJECTIVE_TYPE_BUILD_WALL);
}

void Game::goTo() {
  setObjective(OBJECTIVE_TYPE_GOTO);
}

void Game::buildDoor() {
  setObjective(OBJECTIVE_TYPE_BUILD_DOOR);
}

void Game::attack() {
  setObjective(OBJECTIVE_TYPE_ATTACK);
}

void Game::clearScent() {
  for (auto it = getSelectionIterator(); it.hasNext(); it++) {
    it->clearScent();
  }
}

void Game::placeTower() {
  int numPlayerTowers = 0;
  for (auto it = towerDict.begin(); it != towerDict.end(); it++) {
    if (it->second->sid == playerSpawnID) numPlayerTowers++;
  }
  for (Objective *o: objectives) {
    if (o->type == OBJECTIVE_TYPE_BUILD_TOWER && !o->started)
      numPlayerTowers++;
  }
  if (numPlayerTowers < MAX_TOWERS) {
    placementW = TOWER_SIZE;
    placementH = TOWER_SIZE;
    placingType = BUILDING_TYPE_TOWER;
    context = GAME_CONTEXT_PLACING;
    mouseMoved(mouseX, mouseY);
  } else {
    std::string tooManyTowersString = "You can only build "+std::to_string(MAX_TOWERS)+" towers.";
    panel->addText(tooManyTowersString.c_str());
  }
}

void Game::placeSubspawner() {
  int numPlayerSubspawners = 0;
  for (Subspawner *s: subspawnerList) {
    if (s->sid == playerSpawnID) numPlayerSubspawners++;
  }
  for (Objective *o: objectives) {
    if (o->type == OBJECTIVE_TYPE_BUILD_SUBSPAWNER && !o->started)
      numPlayerSubspawners++;
  }
  if (numPlayerSubspawners < MAX_SUBSPAWNERS) {
    placementW = SUBSPAWNER_SIZE;
    placementH = SUBSPAWNER_SIZE;
    placingType = BUILDING_TYPE_SUBSPAWNER;
    context = GAME_CONTEXT_PLACING;
    mouseMoved(mouseX, mouseY);
  } else {
    std::string tooManySubspawnersString = "You can only build "+std::to_string(MAX_SUBSPAWNERS)+" subspawners.";
    panel->addText(tooManySubspawnersString.c_str());
  }
}

void Game::setObjective(ObjectiveType oType) {
  if (context == GAME_CONTEXT_SELECTED || context == GAME_CONTEXT_PLACING) {
    Objective *o = new Objective(oType, 255, this, selection);
    objectives.push_back(o);
    context = GAME_CONTEXT_UNSELECTED;
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
      MapUnit *first = mapUnitAt(selectedObjective->region.x, selectedObjective->region.y);
      for (MapUnit::iterator it = first->getIterator(selectedObjective->region.w, selectedObjective->region.h);
	   it.hasNext(); it++) {
	it->objective = nullptr;
      }
      delete selectedObjective;
      selectedObjective = nullptr;
    }
  }
}

void Game::showControls() {
  panel->controlsText();
}

void Game::showBasicInfo() {
  panel->basicInfoText();
}

void Game::showCosts() {
  panel->costsText();
}

void Game::clearPanel() {
  panel->clearText();
}

void Game::setTeamDrawColor(SpawnerID sid) {
  switch (sid) {
  case SPAWNER_ID_GREEN:
    disp->setDrawColor(0,255,0);
    break;
  case SPAWNER_ID_RED:
    disp->setDrawColor(255,0,0);
    break;
  }
}

/* Draw the current game screen based on context */
void Game::draw() {
  disp->fillBlack();
  int lum;
  double lumprop;
  switch(context) {
  case GAME_CONTEXT_CONNECTING:
    disp->drawText("Connecting...",0,0);
    break;
  default:
    MapUnit* first = mapUnitAt(view.x, view.y);
    /* Iterate over view */
    for (MapUnit::iterator iter = first->getIterator(view.w, view.h); \
	 iter.hasNext(); iter++) {
      int scaledX = scaleInt(iter->x - view.x);
      int scaledY = scaleInt(iter->y - view.y);
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
	    lum = (int)(255.0 * (double)iter->scent/255.0);
	    disp->setDrawColor(lum, 0, lum);
	  } else {
	    disp->setDrawColorBlack();
	  }
	  offProp = (double)iter->door->hp/(double)MAX_DOOR_HEALTH;
	  off = (int)(offProp*scale*1.0/4.0); 
	  disp->drawRectFilled(scaledX + (int)(scale/2.0) - off, scaledY + (int)(scale/2.0) - off, 2*off, 2*off);
	}
	break;
      case UNIT_TYPE_SPAWNER:
	setTeamDrawColor(iter->spawner->sid);
	lumprop = (double)iter->hp/(double)SUBSPAWNER_UNIT_COST;
	disp->setDrawColorBrightness(lumprop);
	if (((iter->x + iter->y) % 2) == 0) disp->setDrawColorBrightness(0.5);
	disp->drawRectFilled(scaledX, scaledY, (int)scale, (int)scale);
	break;
      case UNIT_TYPE_WALL:
	lum = (int)(((double)iter->hp/(double)MAX_WALL_HEALTH)*100.0)+155;
	disp->setDrawColor(lum,lum,lum);
	disp->drawRectFilled(scaledX, scaledY, (int)scale, (int)scale);
	break;
      case UNIT_TYPE_EMPTY:
	if (menu->getIfScentsShown()) {
	  lum = (int)(255.0 * (double)iter->scent/255.0);
	  disp->setDrawColor(lum, 0, lum);
	  disp->drawRectFilled(scaledX, scaledY, (int)scale, (int)scale);
	}
	break;
      default:	
	break;
      }
    }
    for (auto it = towerDict.begin(); it != towerDict.end(); it++) {
      setTeamDrawColor(it->second->sid);
      if (it->second->hp < it->second->max_hp) {
	disp->setDrawColorBrightness((double)it->second->hp / (double)it->second->max_hp);
      }
      int x = scaleInt(it->second->region.x-view.x);
      int y = scaleInt(it->second->region.y-view.y);
      int s = scaleInt(TOWER_SIZE);
      disp->drawRectFilled(x, y+s/4, s, s/2);
      disp->drawRectFilled(x+s/4, y, s/2, s);
      disp->drawLine(x, y+s/4, x+s/4, y);
      disp->drawLine(x+s-s/4, y, x+s, y+s/4);
      disp->drawLine(x+s, y+s-s/4, x+s-s/4, y+s);
      disp->drawLine(x+s/4, y+s, x, y+s-s/4);
    }
    for (auto it = towerZaps.begin(); it != towerZaps.end(); it++) {
      disp->setDrawColorWhite();
      int x1 = scaleInt(it->x1 - view.x);
      int y1 = scaleInt(it->y1 - view.y);
      int x2 = scaleInt(it->x2 - view.x);
      int y2 = scaleInt(it->y2 - view.y);
      SDL_Point effectPoints[ZAP_EFFECTS_SUBDIVISION];
      for (int i = 0; i < ZAP_EFFECTS_SUBDIVISION; i++) {
	double t = (double)i/(double)(ZAP_EFFECTS_SUBDIVISION-1);
	int tx = (int)((1.0-t)*(double)x1 + t*(double)x2);
	int ty = (int)((1.0-t)*(double)y1 + t*(double)y2);
	if (i != 0 && i != ZAP_EFFECTS_SUBDIVISION - 1) {
	  tx += (rand() % 2*(int)scale) - (int)scale;
	  ty += (rand() % 2*(int)scale) - (int)scale;
	}
	effectPoints[i] = { tx, ty };
      }
      disp->drawLines(effectPoints, ZAP_EFFECTS_SUBDIVISION);
      //      disp->drawLine(x1, y1, x2, y2);
    }
    if (context != GAME_CONTEXT_UNSELECTED) {
      disp->setDrawColor(150,150,150);
      int selectionScaledX = scaleInt(selection.x - view.x);
      int selectionScaledY = scaleInt(selection.y - view.y);
      disp->drawRect(selectionScaledX, selectionScaledY, scaleInt(selection.w), scaleInt(selection.h));
    }
    if (menu->getIfObjectivesShown()) {
      for (Objective *o: objectives) {
	disp->setDrawColor(100,100,100);
	int scaledX = scaleInt(o->region.x - view.x);
	int scaledY = scaleInt(o->region.y - view.y);
	disp->drawRect(scaledX, scaledY, scaleInt(o->region.w), scaleInt(o->region.h));
      }
      if (selectedObjective) {
	SDL_Texture *texture = objectiveInfoTextures[selectedObjective->type];
	int objectiveInfoWidth;
	int objectiveInfoHeight;
	SDL_QueryTexture(texture, NULL, NULL, &objectiveInfoWidth, &objectiveInfoHeight);
	int x = mouseX + 20;
	int y = mouseY + 10;
	if (x > gameDisplaySize - objectiveInfoWidth) x = gameDisplaySize - objectiveInfoWidth;
	if (y > gameDisplaySize - objectiveInfoHeight) y = gameDisplaySize - objectiveInfoHeight;
	disp->setDrawColorBlack();
	disp->drawRectFilled(x, y, objectiveInfoWidth, objectiveInfoHeight);
	disp->setDrawColorWhite();
	disp->drawRect(x, y, objectiveInfoWidth, objectiveInfoHeight);
	disp->drawTexture(texture, x, y);
      }
    }
    disp->setDrawColorWhite();
    disp->drawRect(0, 0, gameDisplaySize, gameDisplaySize);
    panel->draw();
    menu->draw(disp);
    break;
  }
}

void Game::receiveAgentEvent(AgentEvent *aevent) {
  Agent *a = agentDict[aevent->id];
  Building *build;
  MapUnit *startuptr = a->unit;
  MapUnit *destuptr;
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
    if (destuptr->type == UNIT_TYPE_DOOR) {
      destuptr->door->isEmpty = false;
    } else {
      destuptr->type = UNIT_TYPE_AGENT;
    }
    a->unit = destuptr;
    destuptr->agent = a;
    if (startuptr->type == UNIT_TYPE_DOOR) {
      startuptr->door->isEmpty = true;
    } else {
      startuptr->type = UNIT_TYPE_EMPTY;
    }
    startuptr->agent = nullptr;
    break;
  case AGENT_ACTION_BUILDWALL:
    destuptr->type = UNIT_TYPE_WALL;
    destuptr->hp = STARTING_WALL_HEALTH;
    a->die();
    delete a;
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
      if (destuptr->door->hp == MAX_DOOR_HEALTH) destuptr->door->isEmpty = true;
    }
    a->die();
    delete a;
    break;
  case AGENT_ACTION_BUILDTOWER:
    if (destuptr->type == UNIT_TYPE_EMPTY) {
      newTowerID++;
      Tower *tower = new Tower(this, a->sid, newTowerID, destuptr->x, destuptr->y);
      if (a->sid == playerSpawnID)
	destuptr->objective->started = true;
      tower->sid = a->sid;
      towerDict[newTowerID] = tower;
    } else {
      destuptr->building->hp++;
    }
    a->die();
    delete a;
    break;
  case AGENT_ACTION_BUILDSUBSPAWNER:
    if (destuptr->type == UNIT_TYPE_EMPTY) {
      destuptr->hp = 1;
      destuptr->spawner = spawnerDict[a->sid];
      destuptr->type = UNIT_TYPE_SPAWNER;
      if (destuptr->building == nullptr) {
	Subspawner *subspawner = new Subspawner(this, a->sid, destuptr->x-SUBSPAWNER_SIZE/2, destuptr->y-SUBSPAWNER_SIZE/2);
	if (a->sid == playerSpawnID)
	  destuptr->objective->super->started = true;
	subspawnerList.push_back(subspawner);
      }
    } else {
      destuptr->hp++;
    }
    a->die();
    delete a;
    break;
  case AGENT_ACTION_ATTACK:
    Agent *b;
    switch (destuptr->type) {
    case UNIT_TYPE_SPAWNER:
      destuptr->type = UNIT_TYPE_EMPTY;
      destuptr->spawner = nullptr;
      a->die();
      delete a;
      break;
    case UNIT_TYPE_AGENT:
      b = destuptr->agent;
      b->die();
      delete b;
      a->die();
      delete a;
      break;
    case UNIT_TYPE_WALL:
      destuptr->hp--;
      if (destuptr->hp == 0) destuptr->type = UNIT_TYPE_EMPTY;
      a->die();
      delete a;
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
      a->die();
      delete a;
      break;
    case UNIT_TYPE_BUILDING:
      build = destuptr->building;
      build->hp--;
      if (build->hp == 0) {
	for (MapUnit::iterator it = build->getIterator(); it.hasNext(); it++) {
	  it->type = UNIT_TYPE_EMPTY;
	  it->building = nullptr;
	}
	if (build->type == BUILDING_TYPE_TOWER) {
	  towerDict.erase(((Tower *)build)->tid);
	}
	delete build;
      }
      a->die();
      delete a;
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
      addTowerZap(tevent->tid, a->unit->x, a->unit->y);
      a->die();
      delete a;
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
    if (sevent->id >= newAgentID) newAgentID = sevent->id + 1;
    if (sevent->sid == playerSpawnID) numPlayerAgents++;
  }
}

void Game::receiveEvents(Events *events, int numAgentEvents) {
  for (int i = 0; i < numAgentEvents; i++) {
    receiveAgentEvent(&events->agentEvents[i]);
  }
  for (int i = 0; i < MAX_TOWERS; i++) {
    receiveTowerEvent(&events->towerEvents[i]);
  }
  for (int i = 0; i < MAX_SUBSPAWNERS + 1; i++) {
    receiveSpawnerEvent(&events->spawnEvents[i]);
  }
}

void Game::addTowerZap(TowerID tid, int x, int y) {
  Tower *tower = towerDict[tid];
  int tx = tower->region.x + (tower->region.w/2);
  int ty = tower->region.y + (tower->region.h/2);
  TowerZap t = {tx, ty, x, y};
  towerZaps.push_back(t);
}

AgentID Game::getNewAgentID() {
  return newAgentID++;
}

void Game::checkSpawnersDestroyed() {
  auto ssit = subspawnerList.begin();
  while (ssit != subspawnerList.end()) {
    if ((*ssit)->isDestroyed()) {
      for (MapUnit::iterator m = (*ssit)->getIterator(); m.hasNext(); m++) {
	m->building = nullptr;
      }
      delete (*ssit);
      ssit = subspawnerList.erase(ssit);
    } else ssit++;
  }
  for (auto it = spawnerDict.begin(); it != spawnerDict.end(); it++) {
    if (it->second->isDestroyed()) {
      context = GAME_CONTEXT_EXIT;
      std::string reason;
      switch (it->first) {
      case SPAWNER_ID_GREEN:
	reason = "RED team wins!";
	break;
      case SPAWNER_ID_RED:
	reason = "GREEN team wins!";
	break;
      }
      net->closeConnection(reason.c_str());
    }
  }
}

void Game::resign()  {

}

void Game::receiveData(void* data, int numBytes) {
  Events *events = (Events*)data;
  int bytesInAgentEventArray = numBytes - (sizeof(SpawnerEvent) * (MAX_SUBSPAWNERS+1)) - (sizeof(TowerEvent) * MAX_TOWERS);
  int numEvents = bytesInAgentEventArray / sizeof(AgentEvent);
  receiveEvents(events, numEvents);
  checkSpawnersDestroyed();
  if (context != GAME_CONTEXT_EXIT) update();
}

/* Update errythang */
void Game::update() {
  zapCounter = (zapCounter + 1) % ZAP_CLEAR_TIME;
  if (zapCounter == 0) towerZaps.clear();
  for (MapUnit* u: mapUnits) {
    u->marked = false;
    u->update();
    u->objective = nullptr;
  }
  auto it = objectives.begin();
  while (it != objectives.end()) {
    (*it)->update();
    if ((*it)->isDone()) {
      if (selectedObjective == *it) selectedObjective = nullptr;
      delete *it;
      it = objectives.erase(it);
    } else it++;
  }
  int messageSize = (sizeof(SpawnerEvent) * (MAX_SUBSPAWNERS+1)) + (sizeof(TowerEvent) * MAX_TOWERS) + (sizeof(AgentEvent) * numPlayerAgents);
  Events *events = (Events*)malloc(messageSize);
  int i = 0;
  for (auto it = agentDict.begin(); it != agentDict.end(); it++) {
    if (it->second->sid == playerSpawnID) {
      it->second->update(&events->agentEvents[i]);
      i++;
    }
  }
  for (i = 0; i < MAX_TOWERS; i++) events->towerEvents[i].destroyed = false;
  i = 0;
  for (auto it = towerDict.begin(); it != towerDict.end(); it++) {
    if (it->second->sid == playerSpawnID) {
      it->second->update(&events->towerEvents[i]);
      i++;
    }
  }
  for (i = 0; i < MAX_SUBSPAWNERS+1; i++) events->spawnEvents[i].created = false;
  i = 0;
  for (Subspawner *s: subspawnerList) {
    if (s->sid == playerSpawnID) {
      s->update(&events->spawnEvents[i]);
    }
  }
  spawnerDict[playerSpawnID]->update(&events->spawnEvents[MAX_SUBSPAWNERS]);
  receiveEvents(events, numPlayerAgents);
  net->send((void *)events, messageSize);
  checkSpawnersDestroyed();
  free(events);
}

void Game::handleSDLEvent(SDL_Event *e) {
  int x, y;
  if (e->type == SDL_QUIT) {
    context = GAME_CONTEXT_EXIT;
    return;
  }
  /* Handle other events case by case */
  SDL_GetMouseState(&x, &y);
  switch(e->type) {
  case SDL_MOUSEWHEEL:
    if (x >= gameDisplaySize) {
      if (e->wheel.y > 0) panel->scrollDown();
      else if (e->wheel.y < 0) panel->scrollUp();
      break;
    }
    if (y >= gameDisplaySize) break;
    if (e->wheel.y > 0) zoomIn();
    else if (e->wheel.y < 0) zoomOut();
    break;
  case SDL_MOUSEMOTION:
    mouseMoved(x, y);
    break;
  case SDL_MOUSEBUTTONDOWN:
    switch(e->button.button) {
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
    switch(e->button.button) {
    case SDL_BUTTON_LEFT:
      leftMouseUp(x, y);
      break;
    }
    mouseMoved(x,y);
    break;
  case SDL_KEYDOWN:
    switch(e->key.keysym.sym) {
    case SDLK_SPACE:
      break;
    case SDLK_UP:
      panViewUp();
      mouseMoved(x,y);
      break;
    case SDLK_DOWN:
      panViewDown();
      mouseMoved(x,y);
      break;
    case SDLK_RIGHT:
      panViewRight();
      mouseMoved(x,y);
      break;
    case SDLK_LEFT:
      panViewLeft();
      mouseMoved(x,y);
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

/* Main loop of the game */
void Game::mainLoop(void) {
  switch(context) {
  case GAME_CONTEXT_CONNECTING:
    if (net->readyForGame()) {
      context = GAME_CONTEXT_UNSELECTED;
    }
    break;
  default:
    break;
  }
  SDL_Event e;
  if (SDL_PollEvent(&e) != 0) {
    handleSDLEvent(&e);
  }
  draw();
  disp->update();
}

Game::~Game() {
  /* Reset inter-mapunit links, so we don't have bad pointer segfaults on deletion */
  for (MapUnit* u : mapUnits) {
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
  for (auto it = spawnerDict.begin(); it != spawnerDict.end(); it++) {
    delete it->second;
  }
  for (auto it = towerDict.begin(); it != towerDict.end(); it++) {
    delete it->second;
  }
  for (auto it = objectiveInfoTextures.begin(); it != objectiveInfoTextures.end(); it++) {
    SDL_DestroyTexture(it->second);
  }
  towerDict.clear();
  towerZaps.clear();
  objectiveInfoTextures.clear();
  agentDict.clear();
  spawnerDict.clear();
  mapUnits.clear();
  delete disp;
  delete net;
  delete menu;
  delete panel;
}
