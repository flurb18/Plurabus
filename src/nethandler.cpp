#include "nethandler.h"

#ifdef __EMSCRIPTEN__

#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>
#include <emscripten.h>

#endif

#include <iostream>
#include <string>

#include "game.h"
#include "panel.h"


#ifdef __EMSCRIPTEN__

EM_BOOL onOpen(int eventType, const EmscriptenWebSocketOpenEvent *event, void *data) {
  NetHandler *net = (NetHandler*)data;
  net->notifyOpen();
  return EM_TRUE;
}

EM_BOOL onMessage(int eventType, const EmscriptenWebSocketMessageEvent *event, void *data) {
  NetHandler *net = (NetHandler*)data;
  net->receive((void*)event->data, event->numBytes, event->isText);
  return EM_TRUE;
}

EM_BOOL onClose(int eventType, const EmscriptenWebSocketCloseEvent *event, void *data) {
  NetHandler *net = (NetHandler*)data;
  net->notifyClosed((const char*)event->reason);
  return EM_TRUE;
}

#else

context_ptr on_tls_init() {
  return websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv1);/*
    // establishes a SSL connection
    context_ptr ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);

    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                         boost::asio::ssl::context::no_sslv2 |
                         boost::asio::ssl::context::no_sslv3 |
                         boost::asio::ssl::context::single_dh_use);
    } catch (std::exception &e) {
        std::cout << "Error in context pointer: " << e.what() << std::endl;
    }
    return ctx;*/
}

#endif

NetHandler::NetHandler(Game *g, char *pstr):  ncon(NET_CONTEXT_INIT), game(g) {
  pthread_mutex_init(&netLock, NULL);
  pthread_mutex_lock(&netLock);
  pairString = new char[strlen(pstr)];
  strcpy(pairString, pstr);
  std::string uri = "wss://10.8.0.1:31108";
  
#ifdef __EMSCRIPTEN__
  
  EmscriptenWebSocketCreateAttributes attr = {
    uri.c_str(),
    NULL,
    EM_TRUE
  };
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
  m_client.set_tls_init_handler(bind(&on_tls_init));
  
  websocketpp::lib::error_code ec;
  con = m_client.get_connection(uri, ec);
  m_hdl = con->get_handle();
  if (ec) {
    printf("Initial connection error");
  }
  NetHandler::ptr metadata_ptr(this);
  con->set_open_handler(bind(&NetHandler::on_open,
				 metadata_ptr,
				 &m_client,
				 _1));
  con->set_fail_handler(bind(&NetHandler::on_fail,
				 metadata_ptr,
				 &m_client,
				 _1));
  con->set_message_handler(bind(&NetHandler::on_message,
				    metadata_ptr,
				    &m_client,
				    _1,
				    _2));
  con->set_close_handler(bind(&NetHandler::on_close,
				  metadata_ptr,
				  &m_client,
				  _1));
  
  m_client.connect(con);
  pthread_create(&clientThread, NULL, &NetHandler::client_thread, this);
  
#endif

}

#ifndef __EMSCRIPTEN__

void NetHandler::on_open(client *c, websocketpp::connection_hdl hdl) {
  notifyOpen();
}

void NetHandler::on_fail(client *c, websocketpp::connection_hdl hdl) {
  printf(":(");
}

void NetHandler::on_message(client *c, websocketpp::connection_hdl hdl, client::message_ptr data) {
  if (data->get_opcode() == websocketpp::frame::opcode::text) {
    receive((void*)data->get_payload().data(), data->get_payload().size(), true);
  } else {
    receive((void*)data->get_payload().data(), data->get_payload().size(), false);
  }
}

void NetHandler::on_close(client *c, websocketpp::connection_hdl hdl) {
  notifyClosed(con->get_remote_close_reason().c_str());
}

void *NetHandler::client_thread(void *n) {
  NetHandler *net = (NetHandler*)n;
  net->m_client.run();
  return NULL;
}

#endif

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

  EMSCRIPTEN_RESULT result = emscripten_websocket_send_binary(sock, data, numBytes);
  if (result) {
    printf("Failed to send data: %d\n", result);
  }

#else

  websocketpp::lib::error_code ec;
  m_client.send(m_hdl, (const void *)data, numBytes, websocketpp::frame::opcode::binary, ec);
  if (ec) {
    std::cout << "> Error sending message" << std::endl;
  }
  
#endif

}

void NetHandler::receive(void *data, int numBytes, bool isText) {
  switch(ncon) {
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
	game->playerSpawnID = SPAWNER_ID_GREEN;
	game->panel->addText("You are the GREEN team.");
	sendText("Set");
      } else if (strcmp((char *)data, "P2") == 0) {
	ncon = NET_CONTEXT_PLAYING;
	game->playerSpawnID = SPAWNER_ID_RED;
	game->panel->addText("You are the RED team.");
	sendText("Set");
	pthread_mutex_unlock(&netLock);
      } else if (strcmp((char *)data, "Go") == 0) {
	ncon = NET_CONTEXT_PLAYING;
	sendText("Start");
	pthread_mutex_unlock(&netLock);
      }
    }
    break;
  case NET_CONTEXT_PLAYING:
    if (isText) {
      pthread_mutex_lock(&game->threadLock);
      game->secondsRemaining--;
      if (game->context == GAME_CONTEXT_STARTUPTIMER) {
	pthread_cond_signal(&game->startupCond);
      }
      pthread_mutex_unlock(&game->threadLock);
    } else {
      pthread_mutex_lock(&game->threadLock);
      game->receiveData(data, numBytes);
      pthread_mutex_unlock(&game->threadLock);
    }
    break;
  default:
    break;
  }
}

void NetHandler::closeConnection(const char *reason) {
  if (ncon != NET_CONTEXT_CLOSED) {
#ifdef __EMSCRIPTEN__
    emscripten_websocket_close(sock, 1000, reason);
#else
    m_client.close(m_hdl, websocketpp::close::status::normal, std::string(reason));
#endif
    game->context = GAME_CONTEXT_DONE;
    ncon = NET_CONTEXT_CLOSED;
  }
}

void NetHandler::notifyOpen() {
  ncon = NET_CONTEXT_CONNECTED;

#ifdef __EMSCRIPTEN__
  sendText(game->token);
#else
  sendText("aidantest");
  sendText("518john");
#endif
  sendText(pairString);
}

void NetHandler::notifyClosed(const char *reason) {
  if (strlen(reason) > 0) {
    game->panel->addText(reason);
  } else {
    game->panel->addText("Connection closed - unspecified reason");
  }
  ncon = NET_CONTEXT_CLOSED;
  game->context = GAME_CONTEXT_DONE;
}

bool NetHandler::readyForGame() {
  return (ncon == NET_CONTEXT_PLAYING);
}

NetHandler::~NetHandler() {
  if (ncon != NET_CONTEXT_CLOSED)
    closeConnection("Cleanup");
  
#ifdef __EMSCRIPTEN__
  emscripten_websocket_delete(sock);
#else
  pthread_join(clientThread, NULL);
#endif

  pthread_mutex_destroy(&netLock);
}

