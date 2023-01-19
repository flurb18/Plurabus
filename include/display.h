#ifndef DISPLAY_H
#define DISPLAY_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

class Display {
private:
  /* Pointers to the SDL window and renderer, and TTF font */
  SDL_Window* window;
  SDL_Renderer* render;
  TTF_Font* font;
  /* File where font was opened from */
  const char* fontFile;
  int width;
  int height;
  int gameDisplaySize;
  int menuSize;
public:
  Display(int,int,int,int,bool);
  ~Display();
  int getWidth();
  int getHeight();
  int getMenuSize();
  int getGameDisplaySize();
  void fillBlack();
  void setDrawColor(int, int, int);
  void setDrawColorWhite();
  void setDrawColorBlack();
  void setDrawColorBrightness(double);
  void drawPixel(int, int);
  void drawLine(int, int, int, int);
  void drawLines(SDL_Point *, int);
  void drawRect(int, int, int, int);
  void drawRect(SDL_Rect*);
  void drawRectFilled(int, int, int, int);
  void drawCircle(int, int, int);
  void drawSurface(SDL_Surface*, int, int, int, int);
  void drawTexture(SDL_Texture*, int, int);
  void drawTexture(SDL_Texture*, int, int, int, int);
  void drawTextureCropped(SDL_Texture*, int, int, SDL_Rect*);
  void drawImage(const char*, int, int, int, int);
  void drawSVG(const char*, int, int, int, int);
  SDL_Texture *cacheTextWrapped(const char*, int);
  SDL_Texture *cacheSurface(SDL_Surface*);
  SDL_Texture *cacheImage(const char*);
  SDL_Texture *cacheImageColored(const char*, int, int, int);
  void drawTextWrapped(const char*, int, int, int);
  void drawText(const char*, int, int);
  void drawTextSizedColored(const char*, int, int, int, int, int, int);
  void sizeText(const char*, int*, int*);
  void sizeTextSized(const char*, int, int*, int*);
  void sizeTextWrapped(const char*, int, int*, int*);
  void update();
  void wait(int);
};

#endif
