#include "panel.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstring>
#include <iterator>
#include <string>

#include "constants.h"
#include "display.h"

Panel::Panel(Display *d)
    : basicInfoAdded(false), controlsAdded(false), costsAdded(false), disp(d) {
  width = disp->getWidth() - disp->getGameDisplaySize();
  if (width > 0) {
    mobile = false;
    bannerSize = width / 3;
    height = disp->getHeight() - bannerSize;
    wrap = width - (2 * PANEL_PADDING);
    bannerTexture = disp->cacheImage("assets/img/banner.png");
  } else {
    mobile = true;
    height =
        disp->getHeight() - disp->getGameDisplaySize() - disp->getMenuSize();
    width = disp->getWidth();
    bannerSize = height / 6;
    height = height - bannerSize;
    wrap = width - (2 * PANEL_PADDING);
    bannerTexture = disp->cacheImage("assets/img/banner_mobile.png");
  }
  offset = 0;
  textHeight = 0;
  newlineTexture = disp->cacheTextWrapped(">", 0);
}

void Panel::addText(const char *input) { queuedStrings.emplace_back(input); }

void Panel::flushText() {
  for (std::string s : queuedStrings) {
    SDL_Texture *texture = disp->cacheTextWrapped(s.c_str(), wrap);
    int textureHeight;
    SDL_QueryTexture(texture, NULL, NULL, NULL, &textureHeight);
    textHeight += textureHeight;
    if (textHeight > height)
      offset = height - textHeight;
    displayedStrings.push_back(texture);
  }
  queuedStrings.clear();
}

void Panel::basicInfoText() {
  if (!basicInfoAdded) {
    addText("The large checkered blocks are spawners, and the small dots "
            "continuously spawning around them are agents.");
    addText("Influence your agents indirectly by setting objectives for them "
            "to complete on the map.");
    addText(
        "Select regions of the map by clicking and dragging with the mouse.");
    addText("Once a region is selected, designate that region with an "
            "objective for your agents. See 'Controls' for details.");
    addText("You cannot overlap objectives.");
    addText("You can also build towers that destroy enemy agents, subspawners "
            "that spawn continuously at a slower rate than your base spawner, "
            "and bombs which detonate and take out a large area around them.");
    addText(
        "You can toggle viewing set objectives in the view menu, accessible by "
        "clicking on the eye icon. You can also toggle viewing scents here.");
    addText("Whenever an agent completes an objective such as attacking "
            "something or building a wall, that agent dies. Set objectives "
            "wisely - don't waste agents!");
    addText(
        "Win the game by destroying your opponent's base spawner completely.");
  }
  basicInfoAdded = true;
}

void Panel::controlsText() {
  if (!controlsAdded) {
    addText("All actions that can be done with keybinds can be done through "
            "the triple bars menu as well.");
    addText("Once a region is selected, press:");
    addText("'w' - Designate walls to be built in the region.");
    addText("'d' - Designate doors to be built over any walls in the region.");
    addText("'a' - Designate an attack on the region.");
    addText("'g'- Designate that your agents should go to the region.");
    addText("'c' - Clear all scents in the region.");
    addText("To build a tower, press 't' and then click the mouse where you "
            "want to build it. The area a tower is built on must be completely "
            "empty before construction starts.");
    addText("Press 's' and then click to build a subspawner.");
    addText("Press 'b' and then click to build a bomb.");
    addText("Hover over a set objective, which appears as a yellow rectangle, "
            "and press backspace/delete to remove that objective.");
  }
  controlsAdded = true;
}

void Panel::costsText() {
  if (!costsAdded) {
    std::string wallHealth = std::to_string(MAX_WALL_HEALTH);
    std::string doorHealth = std::to_string(MAX_DOOR_HEALTH);
    std::string towerHealth = std::to_string(MAX_TOWER_HEALTH);
    std::string subspawnerCost = std::to_string(
        SUBSPAWNER_SIZE * SUBSPAWNER_SIZE * SUBSPAWNER_UNIT_COST);
    std::string subspawnerUnitCost = std::to_string(SUBSPAWNER_UNIT_COST);
    std::string subspawnerSize = std::to_string(SUBSPAWNER_SIZE);
    std::string bombHealth = std::to_string(MAX_BOMB_HEALTH);
    std::string wallinfo =
        "Building a wall takes only one agent, but destroying a wall takes " +
        wallHealth + ".";
    std::string doorinfo =
        "Building and destroying doors takes " + doorHealth +
        " agents, and doors must be built on top of preexisting walls.";
    std::string towerinfo =
        "Building and destroying towers takes " + towerHealth + " agents.";
    std::string subspawnerinfo =
        "Subspawners cost " + subspawnerCost +
        " agents to build: " + subspawnerUnitCost +
        " agents for each spawn unit in a square with side length " +
        subspawnerSize +
        ". Spawner units only take one unit to destroy - protect them "
        "carefully.";
    std::string bombinfo = "Bombs take " + bombHealth + " units to activate.";
    addText(wallinfo.c_str());
    addText(doorinfo.c_str());
    addText(towerinfo.c_str());
    addText(subspawnerinfo.c_str());
    addText(bombinfo.c_str());
  }
  costsAdded = true;
}

void Panel::clearText() {
  for (SDL_Texture *texture : displayedStrings) {
    SDL_DestroyTexture(texture);
  }
  basicInfoAdded = false;
  controlsAdded = false;
  costsAdded = false;
  displayedStrings.clear();
  offset = 0;
  textHeight = 0;
}

void Panel::draw() {
  if (mobile) {
    disp->setDrawColorBlack();
    disp->drawRectFilled(0, 0, disp->getWidth(), height + bannerSize);
    disp->setDrawColorWhite();
    disp->drawRect(0, 0, disp->getWidth(), height + bannerSize);
    int y = bannerSize + offset;
    for (std::deque<SDL_Texture *>::iterator it = displayedStrings.begin();
         it != displayedStrings.end(); it++) {
      int textureWidth, textureHeight;
      SDL_QueryTexture(*it, NULL, NULL, &textureWidth, &textureHeight);
      int drawHeight =
          (y + textureHeight >= height + bannerSize ? height + bannerSize - y
                                                    : textureHeight);
      SDL_Rect crop = {0, 0, textureWidth, drawHeight};
      disp->drawTextureCropped(*it, PANEL_PADDING, y, &crop);
      y += textureHeight;
      if (y >= height + bannerSize)
        break;
    }
    disp->setDrawColorBlack();
    disp->drawRectFilled(0, 0, disp->getWidth(), bannerSize);
    disp->drawTexture(bannerTexture, 0, 0, disp->getWidth(), bannerSize);
    disp->setDrawColorWhite();
    disp->drawRect(0, 0, disp->getWidth(), bannerSize);
  } else {
    disp->setDrawColorBlack();
    disp->drawRectFilled(disp->getGameDisplaySize(), 0, width,
                         disp->getHeight());
    disp->setDrawColorWhite();
    disp->drawRect(disp->getGameDisplaySize(), 0, width, disp->getHeight());
    int y = bannerSize + offset;
    for (std::deque<SDL_Texture *>::iterator it = displayedStrings.begin();
         it != displayedStrings.end(); it++) {
      int textureHeight;
      SDL_QueryTexture(*it, NULL, NULL, NULL, &textureHeight);
      disp->drawTexture(newlineTexture, disp->getGameDisplaySize(), y);
      disp->drawTexture(*it, disp->getGameDisplaySize() + PANEL_PADDING, y);
      y += textureHeight;
    }
    disp->setDrawColorBlack();
    disp->drawRectFilled(disp->getGameDisplaySize(), 0, width, bannerSize);
    disp->drawTexture(bannerTexture, disp->getGameDisplaySize(), 0, width,
                      bannerSize);
    disp->setDrawColorWhite();
    disp->drawRect(disp->getGameDisplaySize(), 0, width, bannerSize);
  }
}

void Panel::scrollUp() {
  if (offset > height - textHeight)
    offset -= 10;
}

void Panel::scrollDown() {
  if (offset < 0)
    offset += 10;
}

Panel::~Panel() {
  clearText();
  SDL_DestroyTexture(bannerTexture);
}
