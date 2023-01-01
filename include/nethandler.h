#ifndef NETHANDLER_H
#define NETHANDLER_H

#ifdef __EMSCRIPTEN__

#include <emscripten/websocket.h>

#else

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <string>
#include <map>

typedef websocketpp::config::asio_client::message_type::ptr message_ptr;
typedef websocketpp::client<websocketpp::config::asio_client> client;

#endif

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
  
#ifdef __EMSCRIPTEN__

  EMSCRIPTEN_WEBSOCKET_T sock;

#else

  client m_client;
  client::connection_ptr con;
  websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;
  websocketpp::connection_hdl m_hdl;
  
#endif

public:

#ifndef __EMSCRIPTEN__

  typedef websocketpp::lib::shared_ptr<NetHandler> ptr;
  void on_open(client*, websocketpp::connection_hdl);
  void on_fail(client*, websocketpp::connection_hdl);
  void on_message(client*, websocketpp::connection_hdl, message_ptr);
  void on_close(client*, websocketpp::connection_hdl);

#endif

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
