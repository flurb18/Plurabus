#include "panel.h"

#include <cstring>
#include <string>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <iterator>

#include "constants.h"
#include "display.h"

Panel::Panel(Display *d, int pad): basicInfoAdded(false), controlsAdded(false),
				   padding(pad), disp(d) {
  width = disp->getWidth() - disp->getGameDisplaySize();
  height = disp->getHeight() - PANEL_TITLE_HEIGHT - (2*PANEL_VERTICAL_PADDING);
  wrap = width - (2*padding);
  offset = 0;
  textHeight = 0;
  bannerHeight = width / 3;
  bannerTexture = disp->cacheImage("assets/img/banner.png");
  newlineTexture = disp->cacheTextWrapped(">", 0);
}

void Panel::addText(const char* input) {
  SDL_Texture *texture = disp->cacheTextWrapped(input, wrap);
  int textureHeight;
  SDL_QueryTexture(texture, NULL, NULL, NULL, &textureHeight);
  textHeight += textureHeight;
  if (textHeight > height)
    offset = height - textHeight;
  displayedStrings.push_back(texture);
}

void Panel::basicInfoText() {
  if (!basicInfoAdded) {
    addText("The large checkered blocks are spawners, and the small dots continuously spawning around them are agents.");
    addText("Influence your agents indirectly by setting objectives for them to complete on the map.");
    addText("Select regions of the map by clicking and dragging with the mouse.");
    addText("Once a region is selected, press a key to designate that region with an objective for your agents. See 'Controls' for details.");
    addText("You can toggle viewing set objectives in the view menu, accessible by clicking on the eye icon. You can also toggle viewing scents here.");
    addText("Whenever an agent completes an objective such as attacking something or building a wall, that agent dies. Set objectives wisely - don't waste agents!");
    addText("It takes two agents to destroy a wall, but only one to build it.");
    addText("Doors take five agents to build and destroy, and must be built on top of preexisting walls.");
    addText("Spawner units only take one unit to destroy.");
    addText("Win the game by destroying your opponent's spawner completely.");
  }
  basicInfoAdded = true;
}

void Panel::controlsText() {
  if (!controlsAdded) {
    addText("Once a region is selected, press:");
    addText("w - Designate walls to be built in the region.");
    addText("d - Designate doors to be built over any walls in the region.");
    addText("a - Designate an attack on the region.");
    addText("g - Designate that your agents should go to the region.");
    addText("c - Clear all scents in the region.");
    addText("The triple bars menu can also be used for region selection commands.");
    addText("Hover over a set objective, which appears as a grey rectangle, and press backspace/delete to remove that objective.");
  }
  controlsAdded = true;
}

void Panel::clearText() {
  for (SDL_Texture *texture: displayedStrings) {
    SDL_DestroyTexture(texture);
  }
  displayedStrings.clear();
  offset = 0;
  textHeight = 0;
}

void Panel::draw() {
  disp->setDrawColorBlack();
  disp->drawRectFilled(disp->getGameDisplaySize(), 0, width, disp->getHeight());
  disp->setDrawColorWhite();
  disp->drawRect(disp->getGameDisplaySize(), 0, width, disp->getHeight());
  int y = bannerHeight + PANEL_VERTICAL_PADDING + offset;
  for (std::deque<SDL_Texture *>::iterator it = displayedStrings.begin(); it != displayedStrings.end(); it++) {
    int textureHeight;
    SDL_QueryTexture(*it, NULL, NULL, NULL, &textureHeight);
    disp->drawTexture(newlineTexture, disp->getGameDisplaySize(), y);
    disp->drawTexture(*it, disp->getGameDisplaySize() + padding, y);
    y += textureHeight;
  }
  disp->setDrawColorBlack();
  disp->drawRectFilled(disp->getGameDisplaySize(), 0, width, bannerHeight);
  disp->drawTexture(bannerTexture, disp->getGameDisplaySize(), 0, width, bannerHeight);
  disp->setDrawColorWhite();
  disp->drawRect(disp->getGameDisplaySize(), 0, width, bannerHeight);
}

void Panel::scrollUp() {
  if (offset > height - textHeight) offset-=10;
}

void Panel::scrollDown() {
  if (offset < 0) offset+=10;
}


Panel::~Panel() {
  clearText();
  SDL_DestroyTexture(bannerTexture);
}
