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
  int width, height;
  int padding;
  int wrap;
  int offset;
  int textHeight;
  int titleWidth, titleHeight;
  Display *disp;
  std::deque<SDL_Texture *> displayedStrings;
  SDL_Texture *titleTexture;
  SDL_Texture *newlineTexture;
public:
  void addText(const char *);
  void basicInfoText();
  void controlsText();
  void clearText();
  void draw();
  void scrollUp();
  void scrollDown();
  Panel(Display*, int);
  ~Panel();
};

#endif
