#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define DEFAULT_MAX_WS_CLIENTS 15
#define WATCHDOG_INTERVAL 10000
#define WATCHDOG_TIMEOUT 30000

#define COMM_GET_DOOR_STATE "_getDoorState_"
#define COMM_WATCHDOG "_watchdog_"

// Type aliases
using JsonObj = JsonObjectConst;
typedef void (*CommandHandler)(AsyncWebSocketClient* client, JsonObj data);

// Command entry structure for registering message handlers
struct CommandEntry {
    const char* name;
    CommandHandler handler;
};

struct watchDogEntry {
    bool active;
    AsyncWebSocketClient* clientId;
    unsigned long lastPongTime;
};
// Global WebSocket instances

class WebSocketManager {

    public:
// Function declarations

    /**
     * Initialize the WebSocket server
     * @param srv Pointer to the AsyncWebServer instance
     * @return 0 on success, -1 on failure
     */
    void initializeWebSocket(AsyncWebServer *srv);

    /**
     * Register a message handler for a specific command
     * @param command The command name to handle
     * @param handler The callback function to invoke when command is received
     * @return 0 on success, -1 on failure
     */
    int registerMessageHandler(const char* command, CommandHandler handler);

    /**
     * Send a message to all connected WebSocket clients
     * @param command The command name
     * @param payload The JSON payload
     */
    void notifyClients(const char* command, JsonDocument payload);

    /**
     * Send a message to specific connected WebSocket client
     * @param changeString The JSON string to send
     */
    void sendMessage(AsyncWebSocketClient* client, const char* command, JsonDocument payload);

    /**
     * Handle incoming WebSocket messages
     * @param sender The client that sent the message
     * @param arg Frame information
     * @param data The message data
     * @param len The length of the message
     */
    void handleWebSocketMessage(AsyncWebSocketClient *sender, void *arg, uint8_t *data, size_t len);

    /**
     * WebSocket event handler
     * @param server The AsyncWebSocket server instance
     * @param client The connected client
     * @param type The event type (connect, disconnect, data, etc.)
     * @param arg Event-specific argument
     * @param data Event data
     * @param len Length of event data
     */
    void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                void *arg, uint8_t *data, size_t len);

    /**
     * Removes timed out clients from the WebSocket server
     */
    
    void cleanupConnections(); // Function to check for timed-out clients and clean up connections

    private:
    watchDogEntry watchDogEntries[DEFAULT_MAX_WS_CLIENTS]; // Track last pong time for each client for watchdog purposes
    int freeWatchdogEntry = 0;  // to speeedup cleints lookup
    
    void handleWatchdogResponse(AsyncWebSocketClient* client, JsonObj data); // Handler for watchdog response
    void watchdogRemoveEntryIdx(int clientIdx);                             // Function to feed the watchdog timer, if needed
    void watchdogRemoveEntry(AsyncWebSocketClient* client);                   // Function to remove a client from the watchdog tracking
    
    AsyncWebSocket* ws;
    AsyncWebServer* server;
    unsigned long nextWatchdogFeedTime;
    const unsigned long watchdogFeedInterval = WATCHDOG_INTERVAL; // Feed watchdog every 10 seconds
    const unsigned long watchdogTimeout = WATCHDOG_TIMEOUT; // Consider client disconnected if no pong received in 30 seconds

};

#endif // WEBSOCKET_H