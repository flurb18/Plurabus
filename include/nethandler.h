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
  typedef websocketpp::client<websocketpp::config::asio_client> client;
  client m_client;
  websocketpp::connection_hdl m_hdl;
  void on_open(websocketpp::connection_hdl);
  void on_fail(websocketpp::connection_hdl);
  void on_message(websocketpp::connection_hdl, message_ptr);
  void on_close(websocketpp::connection_hdl);
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
