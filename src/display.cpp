#include "display.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <iostream>
#include <string.h>

#include "constants.h"
#include "game.h"

// Initialize a display with a game window size (square), a menu size, and a font size
// Height = game size + menu size
Display::Display(int gameSize, int mSize, int pSize, int fontSize) :
  width(gameSize+pSize), height(gameSize+mSize), gameDisplaySize(gameSize), menuSize(mSize) {
  // Initialize SDL, and check if it initialized correctly
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL not initialized correctly\n";
    std::cerr << SDL_GetError() << std::endl;
    throw SDL_GetError();
  }
  if (TTF_Init() < 0) {
    std::cerr <<  "TTF not initialized correctly\n";
    throw TTF_GetError();
  }
  window = SDL_CreateWindow(
    TITLE,                  //    window title
    SDL_WINDOWPOS_UNDEFINED,           //    initial x position
    SDL_WINDOWPOS_UNDEFINED,           //    initial y position
    width,                               //    width, in pixels
    height,                               //    height, in pixels
    SDL_WINDOW_SHOWN|SDL_WINDOW_OPENGL //    flags
  );
  // Check that the window was successfully made
  if (window == nullptr){
    // In the event that the window could not be made...
    std::cerr << "Could not create window!\n";
    std::cerr << SDL_GetError() << std::endl;
    throw SDL_GetError();
  }
  render = SDL_CreateRenderer(window, -1, 0);
  //fontFile = "/usr/share/fonts/TTF/DejaVuSansMono.ttf";
  fontFile = "assets/NotoSansMono-Regular.ttf";
  font = TTF_OpenFont(fontFile, fontSize);
  if (font == nullptr) {
    std::cerr << "Font could not be loaded!\n";
    std::cerr << TTF_GetError() << std::endl;
    throw TTF_GetError();
  }
}

int Display::getWidth() {
  return width;
}

int Display::getHeight() {
  return height;
}

int Display::getGameDisplaySize() {
  return gameDisplaySize;
}

int Display::getMenuSize() {
  return menuSize;
}

/* Fill the display with black */
void Display::fillBlack() {
  setDrawColorBlack();
  // Clear the window with the color
  SDL_RenderClear(render);
}

/* Set the draw color to an rgb triple */
void Display::setDrawColor(int r, int g, int b) {
  SDL_SetRenderDrawColor(render, r, g, b, 255);
}

/* Set the draw color to white */
void Display::setDrawColorWhite() {
  setDrawColor(255, 255, 255);
}

/* Set the draw color to black */
void Display::setDrawColorBlack() {
  setDrawColor(0, 0, 0);
}

void Display::setDrawColorBrightness(double prop) {
  unsigned char r, g, b, a;
  SDL_GetRenderDrawColor(render, &r, &g, &b, &a);
  SDL_SetRenderDrawColor(render, (int)(prop*(double)r), (int)(prop*(double)g), (int)(prop*(double)b), a);
}

/* Draw a single pixel at (x,y) */
void Display::drawPixel(int x, int y) {
  SDL_RenderDrawPoint(render, x, y);
}

void Display::drawLine(int x1, int y1, int x2, int y2) {
  SDL_RenderDrawLine(render, x1, y1, x2, y2);
}

void Display::drawLines(SDL_Point *points, int count) {
  SDL_RenderDrawLines(render, points, count);
}

/* Draw a rectangle outline at (x,y) of size (w,h) */
void Display::drawRect(int x, int y, int w, int h) {
  SDL_Rect rect = {x, y, w, h};
  SDL_RenderDrawRect(render, &rect);
}

/* Draw the SDL rectangle */
void Display::drawRect(SDL_Rect* r) {
  SDL_RenderDrawRect(render, r);
}

void Display::drawTexture(SDL_Texture *texture, int x, int y) {
  int w, h;
  SDL_QueryTexture(texture, NULL, NULL, &w, &h);
  SDL_Rect rect = {x, y, w, h};
  SDL_RenderCopy(render, texture, nullptr, &rect);
}

void Display::drawTexture(SDL_Texture *texture, int x, int y, int w, int h) {
  SDL_Rect rect = {x, y, w, h};
  SDL_RenderCopy(render, texture, nullptr, &rect);
}

/* Draw a filled in rectangle at (x,y) of size (w,h) */
void Display::drawRectFilled(int x, int y, int w, int h) {
  SDL_Rect rect = {x, y, w, h};
  SDL_RenderFillRect(render, &rect);
}

/* Midpoint Circle Algorithm */
void Display::drawCircle(int x, int y, int r) {
  int diam = 2*r;
  int i = r-1;
  int j = 0;
  int tx = 1;
  int ty = 1;
  int error = tx - diam;
  while (i >= j) {
    SDL_RenderDrawPoint(render, x+i, y-j);
    SDL_RenderDrawPoint(render, x+i, y+j);
    SDL_RenderDrawPoint(render, x-i, y-j);
    SDL_RenderDrawPoint(render, x-i, y+j);
    SDL_RenderDrawPoint(render, x+j, y-i);
    SDL_RenderDrawPoint(render, x+j, y+i);
    SDL_RenderDrawPoint(render, x-j, y-i);
    SDL_RenderDrawPoint(render, x-j, y+i);
    if (error <= 0) {
      j++;
      error += ty;
      ty += 2;
    } else {
      i--;
      tx += 2;
      error += (tx- diam);
    }
  }
}

SDL_Texture *Display::cacheTextWrapped(const char *text, int wrap) {
  SDL_Color white = {255,255,255};
  SDL_Surface *surface = TTF_RenderText_Blended_Wrapped(font, text, white, wrap);
  SDL_Texture *texture = SDL_CreateTextureFromSurface(render, surface);
  SDL_FreeSurface(surface);
  return texture;
}

SDL_Texture *Display::cacheSurface(SDL_Surface *surface) {
  return SDL_CreateTextureFromSurface(render, surface);
}

SDL_Texture *Display::cacheImage(const char *file) {
  SDL_Surface *surface = IMG_Load(file);
  SDL_Texture *texture = SDL_CreateTextureFromSurface(render, surface);
  SDL_FreeSurface(surface);
  return texture;
}

// changes white in file to specified color
SDL_Texture *Display::cacheImageColored(const char* file, int r, int g, int b) {
  SDL_Surface *surface = IMG_Load(file);
  SDL_PixelFormat *fmt = surface->format;
  int bpp = fmt->BytesPerPixel;
  switch (bpp) {
  case 1:
    for (int i = 0; i < surface->w * surface->h; i++) {
      Uint8 *pixptr = (Uint8*)surface->pixels;
      if (pixptr[i] == SDL_MapRGB(fmt, 0xFF, 0xFF, 0xFF))
        pixptr[i] = SDL_MapRGB(fmt, r, g, b);
    }
    break;
  case 2:
    for (int i = 0; i < surface->w * surface->h; i++) {
      Uint16 *pixptr = (Uint16*)surface->pixels;
      if (pixptr[i] == SDL_MapRGB(fmt, 0xFF, 0xFF, 0xFF))
        pixptr[i] = SDL_MapRGB(fmt, r, g, b);
    }
    break;
  case 4:
    for (int i = 0; i < surface->w * surface->h; i++) {
      Uint32 *pixptr = (Uint32*)surface->pixels;
      if (pixptr[i] == SDL_MapRGB(fmt, 0xFF, 0xFF, 0xFF))
        pixptr[i] = SDL_MapRGB(fmt, r, g, b);
    }
    break;
  default:
    break;
  }
  SDL_Texture *texture = SDL_CreateTextureFromSurface(render, surface);
  SDL_FreeSurface(surface);
  return texture;
}

/* Draw the string text at (x,y) using the preloaded font */
void Display::drawTextWrapped(const char *text, int x, int y, int wrap) {
  SDL_Color white = {255,255,255};
  SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(font, text, white, wrap);
  SDL_Texture* texture = SDL_CreateTextureFromSurface(render, surface);
  SDL_Rect rect = {x, y, surface->w, surface->h};
  SDL_FreeSurface(surface);
  SDL_RenderCopy(render, texture, nullptr, &rect);
  SDL_DestroyTexture(texture);
}

/* Draw the string text at (x,y) using the preloaded font */
void Display::drawText(const char *text, int x, int y) {
  SDL_Color white = {255,255,255};
  SDL_Surface* surface = TTF_RenderText_Blended(font, text, white);
  SDL_Texture* texture = SDL_CreateTextureFromSurface(render, surface);
  SDL_Rect rect = {x, y, surface->w, surface->h};
  SDL_FreeSurface(surface);
  SDL_RenderCopy(render, texture, nullptr, &rect);
  SDL_DestroyTexture(texture);
}

void Display::drawSurface(SDL_Surface *surface, int x, int y, int w, int h) {
  SDL_Texture *texture = SDL_CreateTextureFromSurface(render, surface);
  SDL_Rect rect = { x, y, w, h };
  SDL_RenderCopy(render, texture, nullptr, &rect);
  SDL_DestroyTexture(texture);
}

void Display::drawImage(const char *file, int x, int y, int w, int h) {
  SDL_Surface *surface = IMG_Load(file);
  SDL_Texture *texture = SDL_CreateTextureFromSurface(render, surface);
  SDL_Rect rect = {x, y, w, h};
  SDL_RenderCopy(render, texture, nullptr, &rect);
  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
}

/* Load an SVG from a file and draw it */
void Display::drawSVG(const char *svg, int x, int y, int w, int h) {
  SDL_RWops *rw = SDL_RWFromFile(svg, "r");
  SDL_Surface *surface = IMG_Load_RW(rw, 1);
  SDL_Texture *texture = SDL_CreateTextureFromSurface(render, surface);
  SDL_Rect rect = {x, y, w, h};
  SDL_RenderCopy(render, texture, nullptr, &rect);
  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
}

/* Get what the size of some text will be on the display */
void Display::sizeText(const char *text, int *w, int *h) {
  TTF_SizeText(font, text, w, h);
}

void Display::sizeTextWrapped(const char *text, int wrap, int *w, int *h) {
  SDL_Color white = {255,255,255};
  SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(font, text, white, wrap);
  *w = surface->w;
  *h = surface->h;
  SDL_FreeSurface(surface);
}

/* Refresh the display */
void Display::update() {
  SDL_RenderPresent(render);
}

/* Wait t milliseconds */
void Display::wait(int t) {
  SDL_Delay(t);
}

/* Destroy all SDL and TTF stuff */
Display::~Display() {
  TTF_Quit();
  SDL_DestroyRenderer(render);
  SDL_DestroyWindow(window);
  SDL_Quit();
}
