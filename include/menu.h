
#ifndef MENU_H
#define MENU_H

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

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
  SDL_Surface *icon;
  int positionIndex;
  void (MenuItem::*menuFunc)();
  SubMenu subMenu;
  bool subMenuShown;
  bool isBlank;
  bool highlighted;
public:
  void doNothing();
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
  SDL_Surface *checkIcon;
  std::vector<MenuItem*> items;
  int itemsInView;
  int indexOffset;
public:
  void hideAllSubMenus();
  void draw(Display*);
  bool getIfScentsShown();
  bool getIfObjectivesShown();
  Menu(Game*, int);
  ~Menu();
};

#endif
