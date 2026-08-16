#ifndef WEBSOCKET_LOG_TRANSPORT_H
#define WEBSOCKET_LOG_TRANSPORT_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include "config.h"
#include "logger.h"

#ifndef LOG_SERVER_PATH
#define LOG_SERVER_PATH "/"
#endif

#ifndef LOG_HANDSHAKE_RESPONSE_MAX_LEN
#define LOG_HANDSHAKE_RESPONSE_MAX_LEN 512
#endif

class WebSocketLogTransport : public BufferedLogTransport {
public:
    WebSocketLogTransport(const char* host = LOG_SERVER_IP,
                          uint16_t port = LOG_SERVER_PORT,
                          const char* path = LOG_SERVER_PATH);

    void begin() override;
    void update(unsigned long currentMillis) override;
    bool isConnected() const override;

protected:
    bool reconnect(unsigned long currentMillis) override;
    bool sendNow(const QueuedLogRecord& record) override;

private:
    enum class ConnectionState : uint8_t {
        Disconnected,
        Connecting,
        Handshaking,
        Connected
    };

    AsyncClient client;
    const char* host;
    uint16_t port;
    const char* path;
    ConnectionState state = ConnectionState::Disconnected;
    unsigned long lastActivityMillis = 0;
    char handshakeResponse[LOG_HANDSHAKE_RESPONSE_MAX_LEN + 1] = {};
    size_t handshakeResponseLen = 0;

    static void onConnect(void* arg, AsyncClient* client);
    static void onDisconnect(void* arg, AsyncClient* client);
    static void onData(void* arg, AsyncClient* client, void* data, size_t len);
    static void onError(void* arg, AsyncClient* client, int8_t error);
    static void onTimeout(void* arg, AsyncClient* client, uint32_t time);

    void handleConnect(AsyncClient* client);
    void handleDisconnect();
    void handleData(const char* data, size_t len);
    void sendHandshake();
    void clearHandshakeResponse();
    bool sendTextFrame(const char* text, size_t len);
    bool canWrite(size_t len) const;
    bool writeAll(const uint8_t* data, size_t len);
    void closeClient();
};

#endif // WEBSOCKET_LOG_TRANSPORT_H
