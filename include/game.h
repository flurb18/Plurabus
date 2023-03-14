#ifndef GAME_H
#define GAME_H

#include <vector>
#include <list>
#include <map>
#include <SDL2/SDL.h>
#include <SDL2/SDL_rect.h>

#ifdef ANDROID
#include <jni.h>
#endif

#include "agent.h"
#include "building.h"
#include "event.h"
#include "mapunit.h"
#include "menu.h"
#include "objective.h"

/* Forward declarations */
class Display;
class Spawner;
class NetHandler;
class Panel;

/* Various game contexts */
enum Context {
  GAME_CONTEXT_DONE,
  GAME_CONTEXT_EXIT,
  GAME_CONTEXT_PLAYING,
  GAME_CONTEXT_CONNECTING,
  GAME_CONTEXT_STARTUPTIMER
};

enum SelectionContext {
  SELECTION_CONTEXT_UNSELECTED,
  SELECTION_CONTEXT_SELECTING,
  SELECTION_CONTEXT_SELECTED,
  SELECTION_CONTEXT_PLACING
};

typedef enum DoneStatus {
  DONE_STATUS_INIT,
  DONE_STATUS_WINNER,
  DONE_STATUS_DRAW,
  DONE_STATUS_DISCONNECT,
  DONE_STATUS_RESIGN,
  DONE_STATUS_EXIT,
  DONE_STATUS_BACKGROUND,
  DONE_STATUS_OTHER
} DoneStatus;

typedef struct MarkedCoord {
  int x,y;
} MarkedCoord;

typedef struct TowerZap {
  int x1, y1, x2, y2, counter;
} TowerZap;

typedef struct BombEffect {
  int x, y, counter;
} BombEffect;

class Game {
  friend class NetHandler;
  friend class Agent;
  friend class Spawner;
  friend class Menu;
  friend class Building;
  friend class Tower;
  friend struct Objective;
  friend class MenuItem;
private:
  
#ifdef ANDROID
  JavaVM *jvm;
#endif
  NetHandler *net;
  Menu *menu;
  Panel *panel;
  char *pairString;
  char *token;
  void *eventsBuffer;
  bool mobile;
  bool resignConfirmation;
  bool ended;
  unsigned int eventsBufferCapacity;
  unsigned int numPlayerAgents;
  Context context;
  SelectionContext selectionContext;
  double initScale;
  double scale;
  int gameSize;
  int gameDisplaySize;
  int menuSize;
  int panelSize;
  int panelYDrawOffset;
  int mouseX, mouseY;
  int placementW, placementH;
  int zapCounter;
  int secondsRemaining;
  SDL_Rect selection;
  SDL_Rect view;
  SDL_Texture* bombTextureGreen;
  SDL_Texture* bombTextureRed;
  std::map<ObjectiveType, SDL_Texture*> objectiveInfoTextures;
  std::vector<MapUnit*> mapUnits;
  std::deque<MarkedCoord> markedCoords;
  std::deque<AgentID> markedAgents;
  std::deque<TowerZap> towerZaps;
  std::deque<BombEffect> bombEffects;
  std::list<Objective*> objectives;
  std::map<AgentID, Agent*> agentDict;
  std::map<BuildingType, std::deque<Building*>> buildingLists;
  SpawnerID playerSpawnID;
  SpawnerID winnerSpawnID;
  DoneStatus doneStatus;
  AgentID newAgentID;
  BuildingType placingType;
  MapUnit outside;
  MapUnit* selectedUnit;
  Objective *selectedObjective;
  bool rectCollides(SDL_Rect, SDL_Rect);
  bool potentialSelectionCollidesWithObjective(int, int, int, int);
  bool potentialSelectionCollidesWithBuilding(int, int, int, int);
  void mouseMoved(int, int);
  void leftMouseDown(int, int);
  void leftMouseUp(int, int);
  void rightMouseDown(int, int);
  void panViewLeft();
  void panViewRight();
  void panViewUp();
  void panViewDown();
  void adjustViewToScale();
  void deselect();
  int scaleInt(int);
  MapUnit::iterator getSelectionIterator();
  void attack();
  void buildWall();
  void buildDoor();
  void goTo();
  void placeBuilding(BuildingType);
  void placeTower();
  void placeSubspawner();
  void placeBomb();
  void setObjective(ObjectiveType);
  void clearScent();
  void resign();
  void confirmResign();
  void setTeamDrawColor(SpawnerID);
  void draw();
  void drawBuilding(Building*);
  void drawEffects();
  void drawStartupScreen();
  void standardizeEventCoords(float, float, int*, int*);
  void handleSDLEvent(SDL_Event*);
  void handleSDLEventMobile(SDL_Event*);
  void sizeEventsBuffer(int);
  void receiveData(void*, int);
  void receiveEventsBuffer();
  void sendEventsBuffer();
  void receiveEvents(Events*);
  void receiveAgentEvent(AgentEvent*);
  void receiveTowerEvent(TowerEvent*);
  void receiveSpawnerEvent(SpawnerEvent*);
  void receiveBombEvent(BombEvent*);
  void deleteSelectedObjective();
  void markAgentForDeletion(AgentID);
  void deleteMarkedAgents();
  void checkSpawnersDestroyed();
  void update();
public:
  Display* disp;
  MapUnit* mapUnitAt(int, int);
  Context getContext();
  unsigned long long getTime();
  AgentID getNewAgentID();
  SpawnerID getPlayerSpawnID();
  int getSize();
  void showBasicInfo();
  void showControls();
  void showCosts();
  void clearPanel();
  void zoomIn();
  void zoomOut();
  void end(DoneStatus);
  static int messageSize(int);
  Game(int, int, double, char*, char*, bool);
  ~Game();
  void mainLoop();
};

#endif
