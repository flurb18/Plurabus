#ifndef NETHANDLER_H
#define NETHANDLER_H

#include <emscripten/websocket.h>

/* Forward declarations */
class Game;

/* Various net contexts */
enum NetContext	{
  NET_CONTEXT_INIT,
  NET_CONTEXT_CONNECTED,
  NET_CONTEXT_READY,
  NET_CONTEXT_PLAYING,
  NET_CONTEXT_CLOSED
};

class NetHandler {
private:
  NetContext ncon;
  char *pairString;
  Game *game;
  EMSCRIPTEN_WEBSOCKET_T sock;
public:
  NetHandler(Game*, char*);
  ~NetHandler();
  void sendText(const char*);
  void send(void*, int);
  void receive(void*, int, EM_BOOL);
  void closeConnection(const char*);
  bool readyForGame();
  void notifyOpen();
  void notifyClosed(const char *reason);
};

#endif
