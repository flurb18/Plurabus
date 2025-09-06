#include "nethandler.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>
#endif

#include <pthread.h>

#include <iostream>
#include <string>

#include "game.h"
#include "panel.h"

#ifdef __EMSCRIPTEN__

EM_BOOL onOpen(int eventType, const EmscriptenWebSocketOpenEvent *event,
               void *data) {
  NetHandler *net = (NetHandler *)data;
  net->notifyOpen();
  return EM_TRUE;
}

EM_BOOL onMessage(int eventType, const EmscriptenWebSocketMessageEvent *event,
                  void *data) {
  NetHandler *net = (NetHandler *)data;
  net->receive((void *)event->data, event->numBytes, event->isText);
  return EM_TRUE;
}

EM_BOOL onClose(int eventType, const EmscriptenWebSocketCloseEvent *event,
                void *data) {
  NetHandler *net = (NetHandler *)data;
  net->notifyClosed((const char *)event->reason);
  return EM_TRUE;
}

#else

class NetMetadata {
public:
  NetHandler *net;
  typedef websocketpp::lib::shared_ptr<NetMetadata> ptr;

  NetMetadata(NetHandler *n) : net(n) {}

  void on_open(client *c, websocketpp::connection_hdl hdl) {
    net->notifyOpen();
  }
  void on_message(client *c, websocketpp::connection_hdl hdl,
                  client::message_ptr data) {
    if (data->get_opcode() == websocketpp::frame::opcode::text) {
      net->receive((void *)data->get_payload().data(),
                   data->get_payload().size(), true);
    } else {
      net->receive((void *)data->get_payload().data(),
                   data->get_payload().size(), false);
    }
  }
  void on_close(client *c, websocketpp::connection_hdl hdl) {
    net->notifyClosed(
        c->get_con_from_hdl(hdl)->get_remote_close_reason().c_str());
  }
};

context_ptr on_tls_init() {
  //  return
  //  websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv1);/*
  // establishes a SSL connection
  context_ptr ctx = std::make_shared<boost::asio::ssl::context>(
      boost::asio::ssl::context::tlsv12);

  try {
    ctx->set_options(boost::asio::ssl::context::default_workarounds |
                     boost::asio::ssl::context::no_sslv2 |
                     boost::asio::ssl::context::no_sslv3 |
                     boost::asio::ssl::context::single_dh_use);
  } catch (std::exception &e) {
    std::cout << "Error in context pointer: " << e.what() << std::endl;
  }
  return ctx;
}

#endif

NetHandler::NetHandler(Game *g, char *pstr, char *uriCstr)
    : ncon(NET_CONTEXT_INIT), game(g) {
  //  pthread_mutex_init(&netLock, NULL);
  // pthread_mutex_lock(&netLock);
  pairString = pstr;

  std::string uri(uriCstr);

#ifdef __EMSCRIPTEN__

  EmscriptenWebSocketCreateAttributes attr = {uri.c_str(), NULL, EM_TRUE};
  sock = emscripten_websocket_new(&attr);
  if (sock <= 0) {
    printf("Socket creation failed: %d", sock);
  }
  emscripten_websocket_set_onopen_callback(sock, this, onOpen);
  emscripten_websocket_set_onmessage_callback(sock, this, onMessage);
  emscripten_websocket_set_onclose_callback(sock, this, onClose);

#else

  m_client.clear_access_channels(websocketpp::log::alevel::all);
  m_client.clear_error_channels(websocketpp::log::elevel::all);

  m_client.init_asio();
  m_client.start_perpetual();
  m_client.set_tls_init_handler(bind(&on_tls_init));

  websocketpp::lib::error_code ec;
  client::connection_ptr con = m_client.get_connection(uri, ec);
  m_hdl = con->get_handle();
  if (ec) {
    printf("Initial connection error");
  }

  NetMetadata::ptr metadata_ptr(new NetMetadata(this));
  con->set_open_handler(
      bind(&NetMetadata::on_open, metadata_ptr, &m_client, _1));
  con->set_message_handler(
      bind(&NetMetadata::on_message, metadata_ptr, &m_client, _1, _2));
  con->set_close_handler(
      bind(&NetMetadata::on_close, metadata_ptr, &m_client, _1));

  m_client.connect(con);
  m_thread.reset(new websocketpp::lib::thread(&client::run, &m_client));
#endif
}

void NetHandler::sendText(const char *text) {

#ifdef __EMSCRIPTEN__

  EMSCRIPTEN_RESULT result = emscripten_websocket_send_utf8_text(sock, text);
  if (result) {
    printf("Failed to send text: %d\n", result);
  }

#else

  websocketpp::lib::error_code ec;
  m_client.send(m_hdl, std::string(text), websocketpp::frame::opcode::text, ec);
  if (ec) {
    std::cout << "Error sending message" << std::endl;
  }

#endif
}

void NetHandler::send(void *data, int numBytes) {

#ifdef __EMSCRIPTEN__

  EMSCRIPTEN_RESULT result =
      emscripten_websocket_send_binary(sock, data, numBytes);
  if (result) {
    printf("Failed to send data: %d\n", result);
  }

#else

  websocketpp::lib::error_code ec;
  m_client.send(m_hdl, (const void *)data, numBytes,
                websocketpp::frame::opcode::binary, ec);
  if (ec) {
    std::cout << "> Error sending message" << std::endl;
  }

#endif
}

void NetHandler::receive(void *data, int numBytes, bool isText) {
  pthread_mutex_lock(&game->threadLock);
  switch (ncon) {
  case NET_CONTEXT_INIT:
    break;
  case NET_CONTEXT_CONNECTED:
    if (isText) {
      if (strcmp((char *)data, pairString) == 0) {
        ncon = NET_CONTEXT_READY;
        sendText("Ready");
      }
    }
    break;
  case NET_CONTEXT_READY:
    if (isText) {
      if (strcmp((char *)data, "P1") == 0) {
        game->playerSpawnID = SPAWNER_ID_ONE;
        game->panel->addText("You are the GREEN team.");
        game->context = GAME_CONTEXT_STARTUPTIMER;
        game->recvCounter = 4;
        sendText("Set");
      } else if (strcmp((char *)data, "P2") == 0) {
        ncon = NET_CONTEXT_PLAYING;
        game->playerSpawnID = SPAWNER_ID_TWO;
        game->panel->addText("You are the RED team.");
        game->flipped_X = true;
        game->flipped_Y = true;
        game->context = GAME_CONTEXT_STARTUPTIMER;
        game->recvCounter = 1;
        sendText("Set");
      } else if (strcmp((char *)data, "P3") == 0) {
        ncon = NET_CONTEXT_PLAYING;
        game->playerSpawnID = SPAWNER_ID_THREE;
        game->panel->addText("You are the BLUE team.");
        game->flipped_X = true;
        game->context = GAME_CONTEXT_STARTUPTIMER;
        game->recvCounter = 2;
        sendText("Set");
      } else if (strcmp((char *)data, "P4") == 0) {
        ncon = NET_CONTEXT_PLAYING;
        game->playerSpawnID = SPAWNER_ID_FOUR;
        game->panel->addText("You are the YELLOW team.");
        game->flipped_Y = true;
        game->context = GAME_CONTEXT_STARTUPTIMER;
        game->recvCounter = 3;
        sendText("Set");
      } else if (strcmp((char *)data, "Go") == 0) {
        ncon = NET_CONTEXT_PLAYING;
        sendText("Start");
      }
    }
    break;
  case NET_CONTEXT_PLAYING:
    if (isText) {
      if (strcmp((char *)data, "TIMER") == 0) {
        if (game->secondsRemaining-- - GAME_TIME_SECONDS == 0) {
          game->context = GAME_CONTEXT_PLAYING;
          if (game->playerSpawnID == SPAWNER_ID_ONE) {
            game->update();
            game->receiveEventsBuffer();
            game->sendEventsBuffer();
          }
        }
      } else if (strcmp((char *)data, "WINNER_1") == 0) {
        game->winnerSpawnID = SPAWNER_ID_ONE;
        game->end(DONE_STATUS_WINRECV);
      } else if (strcmp((char *)data, "WINNER_2") == 0) {
        game->winnerSpawnID = SPAWNER_ID_TWO;
        game->end(DONE_STATUS_WINRECV);
      } else if (strcmp((char *)data, "TIMEOUT") == 0) {
        game->end(DONE_STATUS_TIMEOUT);
      } else if (strcmp((char *)data, "DISCONNECT") == 0) {
        game->end(DONE_STATUS_DISCONNECT);
      } else if (strcmp((char *)data, "RESIGN") == 0) {
        game->winnerSpawnID = game->playerSpawnID;
        game->end(DONE_STATUS_RESIGN);
      } else if (strcmp((char *)data, "FRAME_TIMEOUT") == 0) {
        game->end(DONE_STATUS_FRAME_TIMEOUT);
      }
    } else {
      game->receiveData(data, numBytes);
    }
    break;
  default:
    break;
  }
  pthread_mutex_unlock(&game->threadLock);
}

void NetHandler::closeConnection(const char *reason) {
  if (ncon != NET_CONTEXT_CLOSED) {
#ifdef __EMSCRIPTEN__
    emscripten_websocket_close(sock, 1000, reason);
#else
    m_client.stop_perpetual();
    m_client.close(m_hdl, websocketpp::close::status::normal,
                   std::string(reason));
#endif
    ncon = NET_CONTEXT_CLOSED;
  }
}

void NetHandler::notifyOpen() {
  ncon = NET_CONTEXT_CONNECTED;

  sendText(pairString);
}

void NetHandler::notifyClosed(const char *reason) { ncon = NET_CONTEXT_CLOSED; }

bool NetHandler::readyForGame() { return (ncon == NET_CONTEXT_PLAYING); }

NetHandler::~NetHandler() {
  if (ncon != NET_CONTEXT_CLOSED)
    closeConnection("Cleanup");
  delete[] pairString;
#ifdef __EMSCRIPTEN__
  emscripten_websocket_delete(sock);
#else
  m_thread->join();
#endif
}
