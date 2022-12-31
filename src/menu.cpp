#include "menu.h"

#include <SDL2/SDL_image.h>

#include "constants.h"
#include "display.h"
#include "game.h"

void SubMenu::size(Display *disp, int relIdx) {
  int maxTextWidth = 0;
  int maxTextHeight = 0;
  for (auto it = strings.begin(); it != strings.end(); it++) {
    int textWidth, textHeight;
    disp->sizeText(*it, &textWidth, &textHeight);
    if (textWidth > maxTextWidth) maxTextWidth = textWidth;
    if (textHeight > maxTextHeight) maxTextHeight = textHeight;
  }
  w = maxTextWidth + (SUBMENU_HORIZONTAL_PADDING * 2);
  if (isToggleSubMenu) w += maxTextHeight;
  h = maxTextHeight * strings.size();
  int itemSize = disp->getMenuSize();
  x = relIdx * itemSize + (itemSize / 2) - (w / 2);
  if (x > disp->getGameDisplaySize() - w) x = disp->getGameDisplaySize() - w;
  y = disp->getGameDisplaySize() - h;
}

SubMenu::~SubMenu() {
  strings.clear();
  funcs.clear();
  toggleFlags.clear();
}

MenuItem::MenuItem(Game *g, int i):
  game(g),
  positionIndex(i),
  subMenuShown(false),
  isBlank(true),
  highlighted(false) {
  menuFunc = &MenuItem::doNothing;
}

MenuItem::MenuItem(Game *g, const char *iconFile, void (MenuItem::*f)(),int i):
  game(g),
  positionIndex(i),
  subMenuShown(false),
  isBlank(false),
  highlighted(false) {
  icon = game->disp->cacheImage(iconFile);
  menuFunc = f;
}

MenuItem::MenuItem(Game *g, const char *iconFile, SubMenu sub, int i):
  game(g),
  positionIndex(i),
  subMenu(sub),
  subMenuShown(false),
  isBlank(false),
  highlighted(false) {
  icon = game->disp->cacheImage(iconFile);
  menuFunc = &MenuItem::toggleSubMenu;
}

void MenuItem::doNothing() {}

void MenuItem::zoomInWrapper() {
  game->zoomIn();
}

void MenuItem::zoomOutWrapper() {
  game->zoomOut();
}

void MenuItem::toggleSubMenu() {
  if (!subMenuShown) {
    subMenuShown = true;
    highlighted = true;
  } else {
    subMenuShown = false;
    highlighted = false;
  }
}

Menu::Menu(Game *g, int n): game(g), itemsInView(n), indexOffset(0) {
  checkIcon = game->disp->cacheImage("assets/img/check.png");
  SubMenu barsSubMenu;
  int barsSubIdx = 4;
  barsSubMenu.strings.push_back("Wall");
  barsSubMenu.strings.push_back("Door");
  barsSubMenu.strings.push_back("Tower");
  barsSubMenu.strings.push_back("Subspawner");
  barsSubMenu.strings.push_back("Go To");
  barsSubMenu.strings.push_back("Attack");
  barsSubMenu.strings.push_back("Clear Scent");
  barsSubMenu.funcs.push_back(&Game::buildWall);
  barsSubMenu.funcs.push_back(&Game::buildDoor);
  barsSubMenu.funcs.push_back(&Game::placeTower);
  barsSubMenu.funcs.push_back(&Game::placeSubspawner);
  barsSubMenu.funcs.push_back(&Game::goTo);
  barsSubMenu.funcs.push_back(&Game::attack);
  barsSubMenu.funcs.push_back(&Game::clearScent);
  barsSubMenu.isToggleSubMenu = false;
  barsSubMenu.size(game->disp, barsSubIdx);
  SubMenu viewSubMenu;
  int viewSubIdx = 3;
  viewSubMenu.strings.push_back("Show Objectives");
  viewSubMenu.strings.push_back("Show Scents");
  viewSubMenu.isToggleSubMenu = true;
  viewSubMenu.toggleFlags.push_back(true);
  viewSubMenu.toggleFlags.push_back(false);
  viewSubMenu.size(game->disp, viewSubIdx);
  SubMenu infoSubMenu;
  int infoSubIdx = 2;
  infoSubMenu.strings.push_back("Basic Info");
  infoSubMenu.strings.push_back("Controls");
  infoSubMenu.strings.push_back("Agent Action Costs");
  infoSubMenu.strings.push_back("Clear Panel");
  infoSubMenu.funcs.push_back(&Game::showBasicInfo);
  infoSubMenu.funcs.push_back(&Game::showControls);
  infoSubMenu.funcs.push_back(&Game::showCosts);
  infoSubMenu.funcs.push_back(&Game::clearPanel);
  infoSubMenu.isToggleSubMenu = false;
  infoSubMenu.size(game->disp, infoSubIdx);
  SubMenu userSubMenu;
  int userSubIdx = 5;
  userSubMenu.strings.push_back("Resign");
  userSubMenu.funcs.push_back(&Game::resign);
  userSubMenu.isToggleSubMenu = false;
  userSubMenu.size(game->disp, userSubIdx);
  items.reserve(6);
  items.push_back(new MenuItem(game, "assets/img/plus.png", &MenuItem::zoomInWrapper, 0));
  items.push_back(new MenuItem(game, "assets/img/minus.png", &MenuItem::zoomOutWrapper, 1));
  items.push_back(new MenuItem(game, "assets/img/info.png", infoSubMenu, infoSubIdx));
  items.push_back(new MenuItem(game, "assets/img/eye.png", viewSubMenu, viewSubIdx));
  items.push_back(new MenuItem(game, "assets/img/bars.png", barsSubMenu, barsSubIdx));
  items.push_back(new MenuItem(game, "assets/img/user.png", userSubMenu, userSubIdx));

}

bool Menu::getIfScentsShown() {
  return items.at(3)->subMenu.toggleFlags.at(1);
}

bool Menu::getIfObjectivesShown() {
  return items.at(3)->subMenu.toggleFlags.at(0);
}

void Menu::hideAllSubMenus() {
  for (MenuItem *item: items) {
    item->subMenuShown = false;
    item->highlighted = false;
  }
}

void Menu::draw(Display *disp) {
  disp->setDrawColorWhite();
  disp->drawRect(0, disp->getGameDisplaySize(), disp->getGameDisplaySize(), disp->getMenuSize());
  for (MenuItem *item : items) {
    int idx = item->positionIndex + indexOffset;
    int itemSize = disp->getMenuSize();
    disp->setDrawColorBlack();
    if (item->highlighted) disp->setDrawColor(50,50,50);
    disp->drawRectFilled(itemSize * idx, disp->getGameDisplaySize(), itemSize, itemSize);
    disp->setDrawColorWhite();
    disp->drawRect(itemSize * idx, disp->getGameDisplaySize(), itemSize, itemSize);
    if (!item->isBlank) {
      disp->setDrawColorWhite();
      disp->drawTexture(item->icon, itemSize * idx, disp->getGameDisplaySize(), itemSize, itemSize);
      if (item->subMenuShown) {
	disp->setDrawColorBlack();
	disp->drawRectFilled(item->subMenu.x, item->subMenu.y, item->subMenu.w, item->subMenu.h);
	disp->setDrawColorWhite();
	disp->drawRect(item->subMenu.x, item->subMenu.y, item->subMenu.w, item->subMenu.h);
	int y = item->subMenu.y;
	int textHeight = item->subMenu.h / item->subMenu.strings.size();
	for (int i = 0; i < item->subMenu.strings.size(); i++) {
	  disp->drawRect(item->subMenu.x, y, item->subMenu.w, textHeight);
	  if (item->subMenu.isToggleSubMenu) {
	    if (item->subMenu.toggleFlags.at(i)) {
	      disp->drawTexture(checkIcon, item->subMenu.x, y, textHeight, textHeight);
	    }
	    disp->drawText(item->subMenu.strings.at(i), item->subMenu.x + textHeight + SUBMENU_HORIZONTAL_PADDING, y);
	  } else {
	    disp->drawText(item->subMenu.strings.at(i), item->subMenu.x + SUBMENU_HORIZONTAL_PADDING, y);
	  }
	  y += textHeight;
	}
      }
    }
  }
}

Menu::~Menu() {
  for (MenuItem *item : items) {
    SDL_DestroyTexture(item->icon);
    delete item;
  }
  SDL_DestroyTexture(checkIcon);
  items.clear();
}
