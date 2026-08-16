#include "websocket_log_transport.h"

#include <ArduinoJson.h>
#include <cstring>
#include <cstdlib>
#include "config.h"

#ifndef LOG_HANDSHAKE_TIMEOUT
#define LOG_HANDSHAKE_TIMEOUT 5000
#endif

#ifndef LOG_IDLE_DISCONNECT_DELAY
#define LOG_IDLE_DISCONNECT_DELAY 1000
#endif

#ifndef LOG_WS_FRAME_MAX_LEN
#define LOG_WS_FRAME_MAX_LEN 512
#endif

WebSocketLogTransport::WebSocketLogTransport(const char* host, uint16_t port, const char* path)
    : host(host), port(port), path(path)
{
}

void WebSocketLogTransport::begin()
{
    BufferedLogTransport::begin();
    state = ConnectionState::Disconnected;
    lastActivityMillis = 0;
    clearHandshakeResponse();

    client.onConnect(&WebSocketLogTransport::onConnect, this);
    client.onDisconnect(&WebSocketLogTransport::onDisconnect, this);
    client.onData(&WebSocketLogTransport::onData, this);
    client.onError(&WebSocketLogTransport::onError, this);
    client.onTimeout(&WebSocketLogTransport::onTimeout, this);
    client.setAckTimeout(3000);
    client.setRxTimeout(10);
}

void WebSocketLogTransport::update(unsigned long currentMillis)
{
    if (state == ConnectionState::Handshaking
        && currentMillis - lastActivityMillis > LOG_HANDSHAKE_TIMEOUT) {
        closeClient();
    }

    if (state == ConnectionState::Disconnected && client.connected()) {
        closeClient();
    }

    BufferedLogTransport::update(currentMillis);

    if (isConnected()
        && !hasPendingOutput()
        && currentMillis - lastActivityMillis > LOG_IDLE_DISCONNECT_DELAY) {
        closeClient();
    }
}

bool WebSocketLogTransport::isConnected() const
{
    return state == ConnectionState::Connected && client.connected();
}

bool WebSocketLogTransport::reconnect(unsigned long currentMillis)
{
    (void)currentMillis;

    if (state == ConnectionState::Connecting || state == ConnectionState::Handshaking) {
        return false;
    }
    if (isConnected()) {
        return true;
    }

    closeClient();
    state = ConnectionState::Connecting;
    lastActivityMillis = millis();

    if (!client.connect(host, port)) {
        state = ConnectionState::Disconnected;
        return false;
    }

    return true;
}

bool WebSocketLogTransport::sendNow(const QueuedLogRecord& record)
{
    if (!isConnected()) {
        return false;
    }

    JsonDocument payload;
    payload["source"] = record.source;
    payload["area"] = record.logArea;
    payload["severity"] = record.severity;
    payload["message"] = record.message;
    payload["timestamp"] = record.timestamp;

    JsonDocument doc;
    doc["_command_"] = COMM_LOG;
    doc["_timestamp_"] = record.timestamp;
    doc["data"] = payload;

    String output;
    serializeJson(doc, output);
    if (!canWrite(output.length() + 8)) {
        return false;
    }
    return sendTextFrame(output.c_str(), output.length());
}

void WebSocketLogTransport::onConnect(void* arg, AsyncClient* client)
{
    static_cast<WebSocketLogTransport*>(arg)->handleConnect(client);
}

void WebSocketLogTransport::onDisconnect(void* arg, AsyncClient* client)
{
    (void)client;
    static_cast<WebSocketLogTransport*>(arg)->handleDisconnect();
}

void WebSocketLogTransport::onData(void* arg, AsyncClient* client, void* data, size_t len)
{
    (void)client;
    static_cast<WebSocketLogTransport*>(arg)->handleData(static_cast<const char*>(data), len);
}

void WebSocketLogTransport::onError(void* arg, AsyncClient* client, int8_t error)
{
    (void)client;
    (void)error;
    static_cast<WebSocketLogTransport*>(arg)->handleDisconnect();
}

void WebSocketLogTransport::onTimeout(void* arg, AsyncClient* client, uint32_t time)
{
    (void)client;
    (void)time;
    static_cast<WebSocketLogTransport*>(arg)->handleDisconnect();
}

void WebSocketLogTransport::handleConnect(AsyncClient* connectedClient)
{
    (void)connectedClient;
    state = ConnectionState::Handshaking;
    lastActivityMillis = millis();
    clearHandshakeResponse();
    sendHandshake();
}

void WebSocketLogTransport::handleDisconnect()
{
    state = ConnectionState::Disconnected;
    clearHandshakeResponse();
}

void WebSocketLogTransport::handleData(const char* data, size_t len)
{
    lastActivityMillis = millis();

    if (state != ConnectionState::Handshaking) {
        return;
    }

    if (handshakeResponseLen + len > LOG_HANDSHAKE_RESPONSE_MAX_LEN) {
        closeClient();
        return;
    }

    memcpy(&handshakeResponse[handshakeResponseLen], data, len);
    handshakeResponseLen += len;
    handshakeResponse[handshakeResponseLen] = '\0';

    String response(handshakeResponse);
    response.toLowerCase();
    if (response.indexOf(" 101 ") >= 0
        && response.indexOf("upgrade: websocket") >= 0) {
        state = ConnectionState::Connected;
        lastActivityMillis = millis();
        clearHandshakeResponse();
        return;
    }

    if (response.indexOf("\r\n\r\n") >= 0) {
        closeClient();
    }
}

void WebSocketLogTransport::sendHandshake()
{
    String request;
    request.reserve(220);
    request += "GET ";
    request += path;
    request += " HTTP/1.1\r\nHost: ";
    request += host;
    request += ":";
    request += String(port);
    request += "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n";
    request += "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n";
    request += "Sec-WebSocket-Version: 13\r\n\r\n";

    if (!writeAll(reinterpret_cast<const uint8_t*>(request.c_str()), request.length())) {
        closeClient();
    }
}

void WebSocketLogTransport::clearHandshakeResponse()
{
    handshakeResponseLen = 0;
    handshakeResponse[0] = '\0';
}

bool WebSocketLogTransport::sendTextFrame(const char* text, size_t len)
{
    if (text == nullptr || len > 65535) {
        return false;
    }

    uint8_t frame[LOG_WS_FRAME_MAX_LEN] = {};
    size_t headerLen = 0;
    frame[headerLen++] = 0x81;

    if (len < 126) {
        frame[headerLen++] = 0x80 | static_cast<uint8_t>(len);
    } else {
        frame[headerLen++] = 0x80 | 126;
        frame[headerLen++] = static_cast<uint8_t>((len >> 8) & 0xff);
        frame[headerLen++] = static_cast<uint8_t>(len & 0xff);
    }

    uint8_t mask[4] = {
        static_cast<uint8_t>(esp_random() & 0xff),
        static_cast<uint8_t>(esp_random() & 0xff),
        static_cast<uint8_t>(esp_random() & 0xff),
        static_cast<uint8_t>(esp_random() & 0xff)
    };

    for (uint8_t i = 0; i < sizeof(mask); i++) {
        frame[headerLen++] = mask[i];
    }

    if (headerLen + len > sizeof(frame)) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        frame[headerLen + i] = static_cast<uint8_t>(text[i]) ^ mask[i % 4];
    }

    if (!writeAll(frame, headerLen + len)) {
        return false;
    }

    lastActivityMillis = millis();
    return true;
}

bool WebSocketLogTransport::canWrite(size_t len) const
{
    return client.connected() && client.canSend() && client.space() >= len;
}

bool WebSocketLogTransport::writeAll(const uint8_t* data, size_t len)
{
    if (!canWrite(len)) {
        return false;
    }

    return client.write(reinterpret_cast<const char*>(data), len) == len;
}

void WebSocketLogTransport::closeClient()
{
    if (client.connected() || client.connecting()) {
        client.close();
    }
    state = ConnectionState::Disconnected;
    clearHandshakeResponse();
}
