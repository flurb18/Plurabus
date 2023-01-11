#ifndef GAME_H
#define GAME_H

#include <vector>
#include <list>
#include <map>
#include <pthread.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_rect.h>

#include "agent.h"
#include "building.h"
#include "event.h"
#include "mapunit.h"
#include "objective.h"

/* Forward declarations */
class Display;
class Spawner;
class Menu;
class NetHandler;
class Panel;

/* Various game contexts */
enum Context {
  GAME_CONTEXT_DONE,
  GAME_CONTEXT_EXIT,
  GAME_CONTEXT_UNSELECTED,
  GAME_CONTEXT_SELECTING,
  GAME_CONTEXT_SELECTED,
  GAME_CONTEXT_PLACING,
  GAME_CONTEXT_CONNECTING,
  GAME_CONTEXT_STARTUPTIMER
};

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
private:
  Menu *menu;
  Panel *panel;
  char *pairString;
  char *token;
  pthread_t netThread;
  pthread_mutex_t threadLock;
  pthread_cond_t startupCond;
  void *eventsBuffer;
  bool readyToSend;
  bool readyToReceive;
  unsigned int eventsBufferCapacity;
  unsigned int numPlayerAgents;
  Context context;
  double initScale;
  double scale;
  int gameSize;
  int gameDisplaySize;
  int menuSize;
  int panelSize;
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
  std::deque<TowerZap> towerZaps;
  std::deque<BombEffect> bombEffects;
  std::list<Objective*> objectives;
  std::map<AgentID, Agent*> agentDict;
  std::map<BuildingType, std::deque<Building*>> buildingLists;
  SpawnerID playerSpawnID;
  SpawnerID winnerSpawnID;
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
  void setTeamDrawColor(SpawnerID);
  void draw();
  void drawBuilding(Building*);
  void drawEffects();
  void drawStartupScreen();
  void handleSDLEvent(SDL_Event*);
  void sizeEventsBuffer(int);
  void receiveData(void*, int);
  void receiveEvents(Events*);
  void receiveAgentEvent(AgentEvent*);
  void receiveTowerEvent(TowerEvent*);
  void receiveSpawnerEvent(SpawnerEvent*);
  void receiveBombEvent(BombEvent*);
  void deleteSelectedObjective();
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
  static void *net_thread(void *);
  static int messageSize(int);
  Game(int, int, double, char*);
  ~Game();
  void mainLoop();
};

#endif
