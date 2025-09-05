#ifndef MENU_H
#define MENU_H

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <SDL2/SDL.h>
#include <vector>

/* Forward declarations */
class SDL_Surface;
class Game;
class Display;
class Menu;

typedef struct SubMenu {
  std::vector<const char *> strings;
  std::vector<void (Game::*)()> funcs;
  bool isToggleSubMenu;
  std::vector<bool> toggleFlags;
  int x, y, w, h;
  void size(Display*, int);
  ~SubMenu();
} SubMenu;

class MenuItem {
  friend class Menu;
  friend class Game;
private:
  Game *game;
  SDL_Texture *icon;
  int positionIndex;
  void (MenuItem::*menuFunc)();
  SubMenu subMenu;
  bool subMenuShown;
  bool isBlank;
  bool highlighted;
public:
  void doNothing();
  void xButton();
  void zoomInWrapper();
  void zoomOutWrapper();
  void toggleSubMenu();
  MenuItem(Game*, int);
  MenuItem(Game*, const char*, void (MenuItem::*)(), int);
  MenuItem(Game*, const char*, SubMenu, int);
};

class Menu {
  friend class Game;
private:
  Game *game;
  SDL_Texture *checkIcon;
  std::vector<MenuItem*> items;
public:
  void hideAllSubMenus();
  void draw();
  bool getIfScentsShown();
  bool getIfObjectivesShown();
  bool getIfBuildingsOutlined();
  Menu(Game*);
  ~Menu();
};

#endif
