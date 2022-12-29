#include "nethandler.h"

#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>
#include <emscripten.h>

#include <iostream>
#include <string>

#include "game.h"
#include "panel.h"
#include "spawner.h"

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

NetHandler::NetHandler(Game *g, char *pstr):  ncon(NET_CONTEXT_INIT), game(g) {
  pairString = new char[strlen(pstr)];
  strcpy(pairString, pstr);
  EmscriptenWebSocketCreateAttributes attr = {
    "ws://10.8.0.1:31108",
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
}

void NetHandler::sendText(const char *text) {
  EMSCRIPTEN_RESULT result = emscripten_websocket_send_utf8_text(sock, text);
  if (result) {
    printf("Failed to send text: %d\n", result);
  }
}

void NetHandler::send(void *data, int numBytes) {
  EMSCRIPTEN_RESULT result = emscripten_websocket_send_binary(sock, data, numBytes);
  if (result) {
    printf("Failed to send data: %d\n", result);
  }
}

void NetHandler::receive(void *data, int numBytes, EM_BOOL isText) {
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
      } else if (strcmp((char *)data, "Go") == 0) {
	ncon = NET_CONTEXT_PLAYING;
	game->update();
      }
    }
    break;
  case NET_CONTEXT_PLAYING:
    game->receiveData(data, numBytes);
    break;
  default:
    break;
  }
}

void NetHandler::closeConnection(const char *reason) {
  emscripten_websocket_close(sock, 1000, reason);
  ncon = NET_CONTEXT_CLOSED;
}

void NetHandler::notifyOpen() {
  ncon = NET_CONTEXT_CONNECTED;
  sendText(pairString);
}

void NetHandler::notifyClosed(const char *reason) {
  if (strlen(reason) > 0) {
    game->panel->addText(reason);
  } else {
    game->panel->addText("Connection closed - unspecified reason");
  }
  ncon = NET_CONTEXT_CLOSED;
}

bool NetHandler::readyForGame() {
  return (ncon == NET_CONTEXT_PLAYING);
}

NetHandler::~NetHandler() {
  emscripten_websocket_delete(sock);
}

