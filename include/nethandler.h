#ifndef NETHANDLER_H
#define NETHANDLER_H

#ifdef __EMSCRIPTEN__

#include <emscripten/websocket.h>

#else

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
typedef std::shared_ptr<boost::asio::ssl::context> context_ptr;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

#endif

#include <pthread.h>

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
  friend class Game;
private:
  NetContext ncon;
  char *pairString;
  Game *game;
  pthread_mutex_t netLock;
  
#ifdef __EMSCRIPTEN__

  EMSCRIPTEN_WEBSOCKET_T sock;

#else

  client m_client;
  websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;
  websocketpp::connection_hdl m_hdl;
  
#endif

public:
  NetHandler(Game*, char*);
  ~NetHandler();
  void sendText(const char*);
  void send(void*, int);
  void receive(void*, int, bool);
  void closeConnection(const char*);
  bool readyForGame();
  void notifyOpen();
  void notifyClosed(const char *reason);
};

#endif
