#ifndef GAME_H
#define GAME_H

#include <pthread.h>

#include <vector>
#include <list>
#include <map>
#include <string>
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
  GAME_CONTEXT_STARTUPTIMER,
  GAME_CONTEXT_PRACTICE
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
  DONE_STATUS_WINRECV,
  DONE_STATUS_DRAW,
  DONE_STATUS_DISCONNECT,
  DONE_STATUS_RESIGN,
  DONE_STATUS_EXIT,
  DONE_STATUS_BACKGROUND,
  DONE_STATUS_TIMEOUT,
  DONE_STATUS_FRAME_TIMEOUT,
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

typedef struct ColorScheme {
  std::string p1name;
  std::string p2name;
  int p1r, p2r;
  int p1g, p2g;
  int p1b, p2b;
} ColorScheme;

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
  void *eventsBuffer;
  bool mobile;
  bool resignConfirmation;
  bool ended;
  unsigned int eventsBufferCapacity;
  Context context;
  SelectionContext selectionContext;
  ColorScheme colorScheme;
  double initScale;
  double scale;
  int gameMode;
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
  SDL_Texture* p1bombTexture;
  SDL_Texture* p2bombTexture;
  std::map<ObjectiveType, SDL_Texture*> objectiveInfoTextures;
  std::vector<MapUnit*> mapUnits;
  std::deque<MarkedCoord> markedCoords;
  std::deque<AgentID> markedAgents;
  std::deque<TowerZap> towerZaps;
  std::deque<BombEffect> bombEffects;
  std::list<Objective*> objectives;
  std::map<AgentID, Agent*> agentDict;
  std::map<SpawnerID, int> numPlayerAgents;
  std::map<BuildingType, std::deque<Building*>> buildingLists;
  SpawnerID playerSpawnID;
  SpawnerID winnerSpawnID;
  DoneStatus doneStatus;
  AgentID newAgentID;
  BuildingType placingType;
  MapUnit outside;
  MapUnit* selectedUnit;
  Objective *selectedObjective;
  pthread_mutex_t threadLock;
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
  void setColors(std::string, std::string, int, int, int, int, int, int);
  void setTeamDrawColor(SpawnerID);
  void draw();
  void drawBuilding(Building*);
  void drawEffects();
  void drawStartupScreen();
  void standardizeEventCoords(float, float, int*, int*);
  void handleSDLEvent(SDL_Event*);
  void handleSDLEventMobile(SDL_Event*);
  void receiveData(void*, int);
  void receiveEventsBuffer();
  void sendEventsBuffer();
  void sizeEventsBuffer(int);
  void receiveAgentEvent(AgentEvent*);
  void receiveTowerEvent(TowerEvent*);
  void receiveSpawnerEvent(SpawnerEvent*);
  void receiveBombEvent(BombEvent*);
  void deleteSelectedObjective();
  void markAgentForDeletion(AgentID);
  void deleteMarkedAgents();
  void checkSpawnersDestroyed();
  void update();
  void simpleAggMode();
  void simpleDefMode();
public:
  Display* disp;
  MapUnit* mapUnitAt(int, int);
  Context getContext();
  unsigned long long getTime();
  AgentID getNewAgentID();
  SpawnerID getPlayerSpawnID();
  int getSize();
  void toggleShowObjectives();
  void toggleShowScents();
  void toggleOutlineBuildings();
  void greenRed();
  void orangeBlue();
  void purpleYellow();
  void pinkBrown();
  void showBasicInfo();
  void showControls();
  void showCosts();
  void clearPanel();
  void zoomIn();
  void zoomOut();
  void end(DoneStatus);
  static int messageSize(int);
  Game(int, int, int, double, char*, char*, bool);
  ~Game();
  void mainLoop();
};

#endif
