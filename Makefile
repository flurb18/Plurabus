CC=g++
FLAGS=-I$(IDIR) -I/usr/include/websocketpp -Wall
LINKFLAGS=$(FLAGS) -lSDL2 -lSDL2_ttf -lSDL2_image
EXECNAME=hivemind

WEBCC=/usr/lib/emscripten/emcc
WEBFLAGS=-s USE_SDL=2 -O3 -s USE_SDL_IMAGE=2 -s USE_SDL_TTF=2 -s SDL2_IMAGE_FORMATS='["png"]' -I$(IDIR)  -Wall
WEBLINKFLAGS=$(WEBFLAGS) -s ALLOW_MEMORY_GROWTH=1 -lwebsocket.js --preload-file assets --use-preload-plugins
WEBEXECNAME=hivemindweb

ODIR = obj
WEBODIR = webobj
SDIR = src
IDIR = include

LIBS=-lSDL2 -lSDL2_ttf -lSDL2_image 

DEPS = $(wildcard $(IDIR)/*.h)

SRC = $(wildcard $(SDIR)/*.cpp)

_OBJ = $(patsubst $(SDIR)/%.cpp, %.o, $(SRC))
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

_WEBOBJ = $(patsubst $(SDIR)/%.cpp, %.o, $(SRC))
WEBOBJ = $(patsubst %,$(WEBODIR)/%,$(_WEBOBJ))

$(ODIR)/%.o: src/%.cpp $(DEPS)
	$(CC) -c -o $@ $< $(FLAGS)

norm $(EXECNAME): $(OBJ)
	$(CC) -o $(EXECNAME) $^ $(LINKFLAGS)

$(WEBODIR)/%.o: src/%.cpp $(DEPS)
	$(WEBCC) -c -o $@ $< $(WEBFLAGS)

web $(WEBEXECNAME): $(WEBOBJ)
	$(WEBCC) -o $(WEBEXECNAME).html $^ $(WEBLINKFLAGS)

all: $(EXECNAME) $(WEBEXECNAME)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o
	rm -f $(WEBODIR)/*.o
	rm -f $(WEBEXECNAME).html $(WEBEXECNAME).js $(WEBEXECNAME).wasm $(WEBEXECNAME).data $(EXECNAME)
	rm -f *~
	rm -f $(SDIR)/*~
	rm -f $(IDIR)/*~

