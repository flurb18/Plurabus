#ifndef PANEL_H
#define PANEL_H

#include <deque>
#include <string>
#include <SDL2/SDL.h>

class Display;

class Panel {
private:
  bool basicInfoAdded;
  bool controlsAdded;
  bool costsAdded;
  int width, height;
  int wrap;
  int offset;
  int textHeight;
  int bannerHeight;
  Display *disp;
  std::deque<SDL_Texture *> displayedStrings;
  SDL_Texture *bannerTexture;
  SDL_Texture *newlineTexture;
public:
  void addText(const char *);
  void basicInfoText();
  void controlsText();
  void costsText();
  void clearText();
  void draw();
  void scrollUp();
  void scrollDown();
  Panel(Display*);
  ~Panel();
};

#endif
