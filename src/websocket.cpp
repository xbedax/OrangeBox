#include "websocket.h"
#include "timeout.h"
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <time.h>
#include <functional>
#include <timeout.h>
#include "logger.h"


AsyncWebSocket wss("/");
 
//AsyncWebSocket* ws = new AsyncWebSocket("/ws");
//AsyncWebServer *server; 

//using JsonObj = JsonObjectConst;
//typedef void (*CommandHandler)(AsyncWebSocketClient* client, JsonObj data);
//using CommandHandler = std::function<void(AsyncWebSocketClient* client, JsonObj data)>;
/*struct MyCommandEntry {
    const char* name;
    MyCommandHandler handler;
};
*/
CommandEntry commands[CMD_COUNT] = {};


void WebSocketManager::notifyClients(const char* command, JsonDocument payload) {
  JsonDocument doc;
  doc["_command_"] = command;  
  doc["_timestamp_"] = static_cast<uint32_t>(time(nullptr));
  doc["data"] = payload;
  String changeString;
  serializeJson(doc, changeString);
  ws->textAll(changeString);
}

uint8_t WebSocketManager::getClientCount() const {
    return ws->count();
} 

void WebSocketManager::sendMessage(AsyncWebSocketClient* client, const char* command, JsonDocument payload) {
  JsonDocument doc;
  doc["_command_"] = command;
  doc["_timestamp_"] = static_cast<uint32_t>(time(nullptr));
  doc["data"] = payload;
  String changeString;
  serializeJson(doc, changeString);
  client->text(changeString);
} 

int WebSocketManager::registerMessageHandler(const char* command, CommandHandler handler) {
    static size_t cmdIndex = 0;

  logger.logPrint(A_REGISTERING_COMMAND_HANDLER_FOR_COMMAND, LOGAREA_COMM, SEVERITY_DEBUG, command);
    if (cmdIndex < sizeof(commands)/sizeof(commands[0])) {
        commands[cmdIndex].name = command;
        commands[cmdIndex].handler = handler;
        cmdIndex++;
        return 0;
    }
    return -1;
}

void WebSocketManager::handleWebSocketMessage(AsyncWebSocketClient *sender, void *arg, uint8_t *data, size_t len) {
  String serializedChanges;
  JsonDocument response;
  JsonDocument request;

    if (len > 0) {
        DeserializationError error = deserializeJson(request, data, len);
        if (error) {
            logger.logPrint(A_FAILED_TO_PARSE_JSON, LOGAREA_COMM, SEVERITY_WARNING);
            return;
        }
        const char* commandReceived = request["_command_"].as<const char*>();
        if (!commandReceived || !*commandReceived || !request["data"].is<JsonObject>()) {
            logger.logPrint(A_INVALID_MESSAGE_ENVELOPE, LOGAREA_COMM, SEVERITY_WARNING);
            return;
        }
        if(0 != strcmp(commandReceived, "_pong_")) { logger.logPrint(A_RECEIVED_COMMAND, LOGAREA_COMM, SEVERITY_DEBUG, commandReceived);   }  //###
           //###
        JsonObj data = request["data"].as<JsonObj>();
        //const size_t CMD_COUNT = sizeof(commands)/sizeof(commands[0]);
        for (size_t i = 0; i < CMD_COUNT; i++) {
            if (commands[i].handler && strcmp(commandReceived, commands[i].name) == 0) {
                if(0 != strcmp(commandReceived, "_pong_")) { logger.logPrint(A_FOUND_HANDLER_FOR_COMMAND, LOGAREA_COMM, SEVERITY_DEBUG, commandReceived);   }  //###
                if (commands[i].handler) {
                    commands[i].handler(sender, data);
                }
                return;
            }
        }
    }
  logger.logPrint(A_RECEIVED_MESSAGE_WITH_UNKNOWN_COMMAND, LOGAREA_COMM, SEVERITY_WARNING);
}

void WebSocketManager::onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      logger.logPrint(A_CLIENT_CONNECTED, LOGAREA_COMM, SEVERITY_CRITICAL, client->id(), client->remoteIP().toString().c_str());
      handleWatchdogResponse(client, JsonObj());
      break;
    case WS_EVT_DISCONNECT:
      logger.logPrint(A_CLIENT_DISCONNECTED, LOGAREA_COMM, SEVERITY_CRITICAL, client->id());
      watchdogRemoveEntry(client); // Adjust index if needed
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(client, arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void WebSocketManager::update(unsigned long currentMillis) {
  if (!watchdogFeedStarted || timeoutElapsed(watchdogFeedStartedAt, watchdogFeedInterval, currentMillis)) {
    watchdogFeedStartedAt = currentMillis;
    watchdogFeedStarted = true;
    watchdogSendRequest();
    cleanupConnections();

  }
}

void WebSocketManager::watchdogSendRequest() {
  JsonDocument payload;
  payload["ping_data"] = "ping";
  notifyClients(COMM_WATCHDOG_PING, payload);
// Historical log site retired during the structured logging migration.
}

void WebSocketManager::watchdogRemoveEntry(AsyncWebSocketClient* client) {
  int cidx;
  for (cidx = 0; cidx < freeWatchdogEntry; cidx++) {
    if (watchDogEntries[cidx].active && watchDogEntries[cidx].clientId == client) {
      watchdogRemoveEntryIdx(cidx);
    }
  }
}

void WebSocketManager::watchdogRemoveEntryIdx(int clientIdx) {
  int fidx = clientIdx;
  if (clientIdx >= freeWatchdogEntry) { 
    logger.logPrint(A_INVALID_CLIENT_INDEX_FOR_WATCHDOG_REMOVAL, LOGAREA_COMM, SEVERITY_ERROR, clientIdx);
    return;
  }
  if (watchDogEntries[clientIdx].active) {
    watchDogEntries[clientIdx].active = false;
    logger.logPrint(A_REMOVED_WATCHDOG_ENTRY_FOR_CLIENT, LOGAREA_COMM, SEVERITY_INFO, watchDogEntries[clientIdx].clientId->id());
  }
  while (fidx < freeWatchdogEntry - 1) {
    watchDogEntries[fidx] = watchDogEntries[fidx + 1];
    fidx++;
  }
  freeWatchdogEntry--;
} // WatchdogRemoveEntryIdx

void WebSocketManager::handleWatchdogResponse(AsyncWebSocketClient* client, JsonObj data) {
    int cidx;
  for (cidx = 0; cidx < freeWatchdogEntry; cidx++) {
    if (watchDogEntries[cidx].active && watchDogEntries[cidx].clientId == client) {
      watchDogEntries[cidx].lastPongTime = millis();
      return;
    }
  }
  if (cidx < DEFAULT_MAX_WS_CLIENTS) {
    watchDogEntries[cidx].active = true;
    watchDogEntries[cidx].clientId = client;
    watchDogEntries[cidx].lastPongTime = millis();
    freeWatchdogEntry++;
  } else {
    logger.logPrint(A_WATCHDOG_LIMIT, LOGAREA_COMM, SEVERITY_ERROR);
  } 
} // handleWatchdogResponse

void WebSocketManager::cleanupConnections() {
  unsigned long currentTime = millis();
  for (int i = 0; i < freeWatchdogEntry; i++) {
    if (watchDogEntries[i].active) {
      if (timeoutElapsed (watchDogEntries[i].lastPongTime, watchdogFeedInterval, currentTime)) {
        logger.logPrint(A_CLIENT_TIMEOUT, LOGAREA_COMM, SEVERITY_WARNING, watchDogEntries[i].clientId->id());
        watchDogEntries[i].clientId->close();
        watchdogRemoveEntryIdx(i);
      }
    }
  }
}

void WebSocketManager::initializeWebSocket(AsyncWebServer *srv) {
  server = srv;
  ws = &wss;
//  ws->onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
//                      void *arg, uint8_t *data, size_t len) {
//    this->onEvent(server, client, type, arg, data, len);
//  });
  ws->onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len) {
    this->onEvent(server, client, type, arg, data, len);
  });
  server->addHandler(ws);
  for (size_t i = 0; i < CMD_COUNT; i++) {
    commands[i].name = nullptr;
    commands[i].handler = nullptr;
  } 
  registerMessageHandler(COMM_WATCHDOG_PONG, [this](AsyncWebSocketClient* client, JsonObj data) {
    this->handleWatchdogResponse(client, data);
  });
}

