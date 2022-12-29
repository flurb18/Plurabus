#ifndef GAME_H
#define GAME_H

#include <vector>
#include <list>
#include <map>
#include <SDL2/SDL.h>
#include <SDL2/SDL_rect.h>

#include "agent.h"
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
  GAME_CONTEXT_EXIT,
  GAME_CONTEXT_UNSELECTED,
  GAME_CONTEXT_SELECTING,
  GAME_CONTEXT_SELECTED,
  GAME_CONTEXT_CONNECTING,
};

typedef struct MarkedCoord {
  int x,y;
} MarkedCoord;

class Game {
  friend class NetHandler;
  friend class Agent;
  friend class Spawner;
  friend class Menu;
private:
  Menu *menu;
  Panel *panel;
  NetHandler *net;
  unsigned int numPlayerAgents;
  Context context;
  double initScale;
  double scale;
  int gameSize;
  int gameDisplaySize;
  int menuSize;
  int menuItemsInView;
  int panelSize;
  int unitLimit;
  int mouseX, mouseY;
  SDL_Rect selection;
  SDL_Rect view;
  std::map<ObjectiveType, SDL_Texture*> objectiveInfoTextures;
  std::vector<MapUnit*> mapUnits;
  std::vector<MarkedCoord> markedCoords;
  std::list<Objective*> objectives;
  std::map<AgentID, Agent*> agentDict;
  std::map<SpawnerID, Spawner*> spawnerDict;
  SpawnerID playerSpawnID;
  AgentID newAgentID;
  MapUnit outside;
  MapUnit* selectedUnit;
  Objective *selectedObjective;
  bool potentialSelectionCollidesWithObjective(int, int, int, int);
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
  void setObjective(ObjectiveType);
  void clearScent();
  void setTeamDrawColor(SpawnerID);
  void draw();
  void handleSDLEvent(SDL_Event*);
  void receiveData(void*, int);
  void receiveEvents(Events*, int);
  void receiveAgentEvent(AgentEvent*);
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
  void clearPanel();
  void zoomIn();
  void zoomOut();
  int getUnitLimit();
  Game(int, int, int, double, int, char*);
  ~Game();
  void mainLoop();
};

#endif
