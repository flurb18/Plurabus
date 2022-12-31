#include "panel.h"

#include <cstring>
#include <string>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <iterator>

#include "constants.h"
#include "display.h"

Panel::Panel(Display *d, int pad): basicInfoAdded(false), controlsAdded(false), costsAdded(false), 
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
    addText("Once a region is selected, designate that region with an objective for your agents. See 'Controls' for details.");
    addText("You can also build towers that destroy enemy agents and subspawners.");
    addText("You can toggle viewing set objectives in the view menu, accessible by clicking on the eye icon. You can also toggle viewing scents here.");
    addText("Whenever an agent completes an objective such as attacking something or building a wall, that agent dies. Set objectives wisely - don't waste agents!");
    addText("Win the game by destroying your opponent's base spawner completely.");
  }
  basicInfoAdded = true;
}

void Panel::controlsText() {
  if (!controlsAdded) {
    addText("All actions that can be done with keybinds can be done through the triple bars menu as well.");
    addText("Hover over a set objective, which appears as a grey rectangle, and press backspace/delete to remove that objective.");
    addText("Once a region is selected, press:");
    addText("'w' - Designate walls to be built in the region.");
    addText("'d' - Designate doors to be built over any walls in the region.");
    addText("'a' - Designate an attack on the region.");
    addText("'g'- Designate that your agents should go to the region.");
    addText("'c' - Clear all scents in the region.");
    addText("To build a tower, press 't' and then click the mouse where you want to build it. The area a tower is built on must be completely empty before construction starts.");
    addText("Similarly press 's' and then click to build a subspawner.");
  }
  controlsAdded = true;
}

void Panel::costsText() {
  if (!costsAdded) {
    addText("Building a wall takes only one agent, but destroying a wall takes 10.");
    addText("Building and destroying doors takes 20 agents, and doors must be built on top of preexisting walls.");
    addText("Building and destroying towers takes 100 agents.");
    addText("Subspawners cost 250 agents to build: 10 agents for each spawn unit in a 5x5 square.");
    addText("Spawner units only take one unit to destroy - protect them carefully.");
  }
  costsAdded = true;
}

void Panel::clearText() {
  for (SDL_Texture *texture: displayedStrings) {
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
