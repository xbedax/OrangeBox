#include "websocket.h"
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// AsyncWebSocket* ws->("/ws");
 
AsyncWebSocket* ws = new AsyncWebSocket("/ws");
AsyncWebServer *server; 

using JsonObj = JsonObjectConst;
typedef void (*CommandHandler)(AsyncWebSocketClient* client, JsonObj data);

/*struct MyCommandEntry {
    const char* name;
    MyCommandHandler handler;
};
*/
CommandEntry commands[] = {};


void WebSocketManager::notifyClients(const char* command, JsonDocument payload) {
  JsonDocument doc;
  doc["_command_"] = command;  
  doc["timestamp"] = millis();
  doc["data"] = payload;
  String changeString;
  serializeJson(doc, changeString);
  ws->textAll(changeString);
}

void WebSocketManager::sendMessage(AsyncWebSocketClient* client, const char* command, JsonDocument payload) {
  JsonDocument doc;
  doc["_command_"] = command;
  doc["timestamp"] = millis();
  doc["data"] = payload;
  String changeString;
  serializeJson(doc, changeString);
  client->text(changeString);
} 

int WebSocketManager::registerMessageHandler(const char* command, CommandHandler handler) {
    static size_t cmdIndex = 0;

  Serial.printf("Registering command handler for command: %s\n", command);
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
        data[len] = 0;
        DeserializationError error = deserializeJson(request, data);
        if (error) {
            Serial.println("Failed to parse JSON");
            return;
        }
        const char* commandReceived = request["_command_"];
        JsonObj data = request["data"].as<JsonObj>();
        const size_t CMD_COUNT = sizeof(commands)/sizeof(commands[0]);
        for (size_t i = 0; i < CMD_COUNT; i++) {
            if (strcmp(commandReceived, commands[i].name) == 0) {
                if (commands[i].handler) {
                    commands[i].handler(sender, data);
                }
                return;
            }
        }
    }
  Serial.println("Received message with unknown command");
  
}

void WebSocketManager::onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      handleWatchdogResponse(client, JsonObj());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
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
    Serial.printf("Invalid client index %d for watchdog removal\n", clientIdx);
    return;
  }
  if (watchDogEntries[clientIdx].active) {
    watchDogEntries[clientIdx].active = false;
    Serial.printf("Removed watchdog entry for client #%u\n", watchDogEntries[clientIdx].clientId->id());
  }
  while (fidx < freeWatchdogEntry - 1) {
    watchDogEntries[fidx] = watchDogEntries[fidx + 1];
    fidx++;
  }
  freeWatchdogEntry--;
}

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
    Serial.println("Watchdog entry limit reached, cannot track new client");
  } 
} 

void WebSocketManager::cleanupConnections() {
  unsigned long currentTime = millis();
  for (int i = 0; i < freeWatchdogEntry; i++) {
    if (watchDogEntries[i].active && (currentTime - watchDogEntries[i].lastPongTime > watchdogFeedInterval)) {
      Serial.printf("Client #%u timed out, closing connection\n", watchDogEntries[i].clientId->id());
      watchDogEntries[i].clientId->close();
      watchdogRemoveEntryIdx(i);
    }
  }
}

void WebSocketManager::initializeWebSocket(AsyncWebServer *srv) {
  server = srv;
  ws->onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len) {
    this->onEvent(server, client, type, arg, data, len);
  });
  server->addHandler(ws);
}

