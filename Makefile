CC=g++
FLAGS=-I$(IDIR) -Wall -D_WEBSOCKETPP_CPP11_STL_ -pthread
LINKFLAGS=$(FLAGS) -lSDL2 -lSDL2_ttf -lSDL2_image -lboost_system -lboost_thread -lboost_random -lssl -lcrypto
EXECNAME=hivemind

WEBCC=/usr/lib/emsdk/upstream/emscripten/emcc
WEBFLAGS=-s USE_SDL=2 -O3 -s USE_SDL_IMAGE=2 -s USE_SDL_TTF=2 -s SDL2_IMAGE_FORMATS='["png"]' -I$(IDIR) -Wall
WEBLINKFLAGS=$(WEBFLAGS) -s ALLOW_MEMORY_GROWTH=1 -lwebsocket.js --embed-file assets --use-preload-plugins -s INITIAL_MEMORY=67108864 -s MAXIMUM_MEMORY=1073741824
WEBEXECNAME=hivemindweb
WEBEXECOUTPUTDIR=web/hivemindweb
WEBEXECOUTPUTEXT=.js .wasm .data
WEBEXECOUTPUTFILES=$(patsubst %,$(WEBEXECNAME)%,$(WEBEXECOUTPUTEXT))
WEBEXECOUTPUTFILESPATH=$(patsubst %,$(WEBEXECOUTPUTDIR)/%,$(WEBEXECOUTPUTFILES))

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

local $(EXECNAME): $(OBJ)
	$(CC) -o $(EXECNAME) $^ $(LINKFLAGS)

$(WEBODIR)/%.o: src/%.cpp $(DEPS)
	$(WEBCC) -c -o $@ $< $(WEBFLAGS)

web $(WEBEXECNAME): $(WEBOBJ)
	$(WEBCC) -o $(WEBEXECOUTPUTDIR)/$(WEBEXECNAME).js $^ $(WEBLINKFLAGS)

all: $(EXECNAME) $(WEBEXECNAME)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o
	rm -f $(WEBODIR)/*.o
	rm -f $(WEBEXECOUTPUTFILESPATH)
	rm -f $(WEBEXECOUTPUTDIR)/*~
	rm -f $(EXECNAME)
	rm -f *~
	rm -f $(SDIR)/*~
	rm -f $(IDIR)/*~

