#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include "config.h"
#include <ESP-Reverse_Tunneling_Libssh2.h>

#ifndef VPN_DIRECT_TEST_HOST
#define VPN_DIRECT_TEST_HOST "127.0.0.1"
#endif
#ifndef VPN_DIRECT_TEST_PORT
#define VPN_DIRECT_TEST_PORT 8089
#endif
#ifndef VPN_DIRECT_TEST_PATH
#define VPN_DIRECT_TEST_PATH "/"
#endif
#ifndef VPN_DIRECT_LARGE_TEST_PATH
#define VPN_DIRECT_LARGE_TEST_PATH ""
#endif
#ifndef VPN_DIRECT_LARGE_TEST_EVERY_N
#define VPN_DIRECT_LARGE_TEST_EVERY_N 0UL
#endif
#ifndef VPN_DIRECT_TEST_INTERVAL_MS
#define VPN_DIRECT_TEST_INTERVAL_MS 11000UL
#endif
#ifndef VPN_MEMORY_LOG_INTERVAL_MS
#define VPN_MEMORY_LOG_INTERVAL_MS 30000UL
#endif
#ifndef VPN_DUPLEX_REVERSE_LOCAL_PORT
#define VPN_DUPLEX_REVERSE_LOCAL_PORT 80
#endif

namespace {
WebServer reverseServer(VPN_DUPLEX_REVERSE_LOCAL_PORT);
SSHTunnel tunnel;
char localHost[16] = "127.0.0.1";
uint32_t nextDirectTestMs = 0;
uint32_t nextMemoryLogMs = 0;
uint32_t directCycle = 0;
const char* reqhostname = BOX_HOST_NAME;

struct HeapSnapshot {
    size_t free8;
    size_t min8;
    size_t largest8;
    size_t freeInternal;
    size_t minInternal;
    size_t largestInternal;
    size_t freePsram;
    size_t minPsram;
    size_t largestPsram;
};

bool hasTrendBaseline = false;
HeapSnapshot trendBaseline = {};
HeapSnapshot previousDirectAfter = {};
uint32_t negativeFreeInternalSteps = 0;
long negativeFreeInternalBytes = 0;
long worstFreeInternalStep = 0;

long heapDelta(size_t before, size_t after)
{
    return static_cast<long>(after) - static_cast<long>(before);
}

const char* tunnelStateName(TunnelState state)
{
    switch (state) {
    case TUNNEL_DISCONNECTED:
        return "Disconnected";
    case TUNNEL_CONNECTING:
        return "Connecting";
    case TUNNEL_CONNECTED:
        return "Connected";
    case TUNNEL_ERROR:
        return "Error";
    default:
        return "Unknown";
    }
}

const char* closeReasonName(ChannelCloseReason reason)
{
    switch (reason) {
    case ChannelCloseReason::RemoteClosed:
        return "remote";
    case ChannelCloseReason::LocalClosed:
        return "local";
    case ChannelCloseReason::Error:
        return "error";
    case ChannelCloseReason::Timeout:
        return "timeout";
    case ChannelCloseReason::Manual:
        return "manual";
    case ChannelCloseReason::Unknown:
    default:
        return "unknown";
    }
}

HeapSnapshot captureHeap()
{
    HeapSnapshot snapshot;
    snapshot.free8 = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    snapshot.min8 = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    snapshot.largest8 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    snapshot.freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    snapshot.minInternal = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    snapshot.largestInternal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    snapshot.freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    snapshot.minPsram = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    snapshot.largestPsram = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    return snapshot;
}

void printHeapSnapshot(const char* tag, const HeapSnapshot& snapshot)
{
    Serial.printf(
        "HEAP tag=%s up=%lu cycle=%lu state=%s active=%d "
        "free8=%u min8=%u largest8=%u "
        "free_int=%u min_int=%u largest_int=%u "
        "free_psram=%u min_psram=%u largest_psram=%u\n",
        tag,
        static_cast<unsigned long>(millis()),
        static_cast<unsigned long>(directCycle),
        tunnelStateName(tunnel.getState()),
        tunnel.getActiveChannels(),
        static_cast<unsigned>(snapshot.free8),
        static_cast<unsigned>(snapshot.min8),
        static_cast<unsigned>(snapshot.largest8),
        static_cast<unsigned>(snapshot.freeInternal),
        static_cast<unsigned>(snapshot.minInternal),
        static_cast<unsigned>(snapshot.largestInternal),
        static_cast<unsigned>(snapshot.freePsram),
        static_cast<unsigned>(snapshot.minPsram),
        static_cast<unsigned>(snapshot.largestPsram));
}

void logMemorySnapshot(const char* tag)
{
    printHeapSnapshot(tag, captureHeap());
}

void logHeapDelta(const HeapSnapshot& before, const HeapSnapshot& after)
{
    Serial.printf(
        "HEAP_DELTA cycle=%lu free8=%ld largest8=%ld free_int=%ld "
        "largest_int=%ld free_psram=%ld largest_psram=%ld\n",
        static_cast<unsigned long>(directCycle),
        static_cast<long>(after.free8) - static_cast<long>(before.free8),
        static_cast<long>(after.largest8) - static_cast<long>(before.largest8),
        static_cast<long>(after.freeInternal) - static_cast<long>(before.freeInternal),
        static_cast<long>(after.largestInternal) - static_cast<long>(before.largestInternal),
        static_cast<long>(after.freePsram) - static_cast<long>(before.freePsram),
        static_cast<long>(after.largestPsram) - static_cast<long>(before.largestPsram));
}

void logHeapTrend(const HeapSnapshot& after)
{
    if (!hasTrendBaseline) {
        trendBaseline = after;
        previousDirectAfter = after;
        hasTrendBaseline = true;
        Serial.printf("HEAP_TREND cycle=%lu baseline=1\n",
                      static_cast<unsigned long>(directCycle));
        return;
    }

    long stepFreeInternal = heapDelta(previousDirectAfter.freeInternal,
                                      after.freeInternal);
    if (stepFreeInternal < 0) {
        negativeFreeInternalSteps++;
        negativeFreeInternalBytes += -stepFreeInternal;
        if (stepFreeInternal < worstFreeInternalStep) {
            worstFreeInternalStep = stepFreeInternal;
        }
    }

    Serial.printf(
        "HEAP_TREND cycle=%lu since_base_free_int=%ld "
        "since_base_largest_int=%ld since_prev_free_int=%ld "
        "since_prev_largest_int=%ld neg_steps=%lu neg_bytes=%ld "
        "worst_step=%ld\n",
        static_cast<unsigned long>(directCycle),
        heapDelta(trendBaseline.freeInternal, after.freeInternal),
        heapDelta(trendBaseline.largestInternal, after.largestInternal),
        stepFreeInternal,
        heapDelta(previousDirectAfter.largestInternal, after.largestInternal),
        static_cast<unsigned long>(negativeFreeInternalSteps),
        negativeFreeInternalBytes,
        worstFreeInternalStep);

    previousDirectAfter = after;
}

const char* directRequestPath()
{
    const uint32_t largeEvery = VPN_DIRECT_LARGE_TEST_EVERY_N;
    if (largeEvery > 0 && (directCycle % largeEvery) == 0 &&
        VPN_DIRECT_LARGE_TEST_PATH[0] != '\0') {
        return VPN_DIRECT_LARGE_TEST_PATH;
    }
    return VPN_DIRECT_TEST_PATH;
}

void logDirectResult(const char* result, size_t bytes)
{
    Serial.printf("DIRECT_RESULT cycle=%lu result=%s bytes=%u\n",
                  static_cast<unsigned long>(directCycle),
                  result,
                  static_cast<unsigned>(bytes));
}

void finishDirectCycle(const char* tag, const HeapSnapshot& before)
{
    HeapSnapshot after = captureHeap();
    printHeapSnapshot(tag, after);
    logHeapDelta(before, after);
    logHeapTrend(after);
}

void onSessionConnected()
{
    logMemorySnapshot("session_connected");
}

void onSessionDisconnected()
{
    logMemorySnapshot("session_disconnected");
}

void onChannelOpened(int channelIndex)
{
    Serial.print("CHANNEL_OPEN ");
    Serial.println(channelIndex);
    logMemorySnapshot("channel_open");
}

void onChannelClosed(int channelIndex, ChannelCloseReason reason)
{
    Serial.print("CHANNEL_CLOSE ");
    Serial.print(channelIndex);
    Serial.print(" reason=");
    Serial.println(closeReasonName(reason));
    logMemorySnapshot("channel_close");
}

void onTunnelError(int code, const char* detail)
{
    Serial.print("TUNNEL_ERROR code=");
    Serial.print(code);
    Serial.print(" detail=");
    Serial.println(detail != nullptr ? detail : "");
    logMemorySnapshot("tunnel_error");
}

void setLocalHostFromWiFi()
{
    IPAddress ip = WiFi.localIP();
    snprintf(localHost, sizeof(localHost), "%u.%u.%u.%u",
             ip[0], ip[1], ip[2], ip[3]);
}

void handleReverseRoot()
{
    String body = "hello from orangebox reverse tunnel\n";
    body += "local=";
    body += localHost;
    body += ":";
    body += VPN_DUPLEX_REVERSE_LOCAL_PORT;
    body += "\n";
    body += "uptime_ms=";
    body += millis();
    body += "\n";
    reverseServer.send(200, "text/plain", body);
}

bool connectWiFi()
{
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(reqhostname);
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);

    Serial.print("WIFI_CONNECT");
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 30000UL) {
        Serial.print(".");
        delay(500);
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WIFI_FAIL");
        return false;
    }

    setLocalHostFromWiFi();
    Serial.print("WIFI_OK ");
    Serial.println(localHost);
    return true;
}

void configureSsh()
{
    globalSSHConfig.setSSHKeyAuthFromMemory(VPN_GATEWAY_HOST,
                                            VPN_GATEWAY_PORT,
                                            VPN_USERNAME,
                                            VPN_PRIVATE_KEY,
                                            VPN_PUBLIC_KEY,
                                            "");
    globalSSHConfig.setHostKeyVerification(VPN_GATEWAY_FINGERPRINT,
                                           VPN_GATEWAY_KEY_TYPE,
                                           true);

    globalSSHConfig.clearTunnelMappings();
    globalSSHConfig.setMaxReverseListeners(1);
    globalSSHConfig.setTunnelConfig("127.0.0.1",
                                    VPN_REMOTE_BIND_PORT,
                                    localHost,
                                    VPN_DUPLEX_REVERSE_LOCAL_PORT);
    globalSSHConfig.setConnectionConfig(VPN_CLIENT_KEEPALIVE_INTERVAL_SEC,
                                        5000,
                                        10,
                                        30);
    globalSSHConfig.setBufferConfig(8192,
                                    5,
                                    1800000,
                                    64 * 1024);
    globalSSHConfig.setDebugConfig(true, 115200);
}

bool connectSsh()
{
    configureSsh();

    if (!tunnel.init()) {
        Serial.println("SSH_INIT_FAIL");
        return false;
    }

    SSHTunnelEvents handlers;
    handlers.onSessionConnected = onSessionConnected;
    handlers.onSessionDisconnected = onSessionDisconnected;
    handlers.onChannelOpened = onChannelOpened;
    handlers.onChannelClosed = onChannelClosed;
    handlers.onError = onTunnelError;
    tunnel.setEventHandlers(handlers);

    if (!tunnel.connectSSH()) {
        Serial.println("SSH_CONNECT_FAIL");
        return false;
    }

    Serial.print("SSH_OK reverse=127.0.0.1:");
    Serial.print(VPN_REMOTE_BIND_PORT);
    Serial.print(" -> ");
    Serial.print(localHost);
    Serial.print(":");
    Serial.println(VPN_DUPLEX_REVERSE_LOCAL_PORT);
    return true;
}

void runDirectHttpTest()
{
    directCycle++;
    HeapSnapshot before = captureHeap();
    printHeapSnapshot("direct_before", before);

    if (!tunnel.isConnected()) {
        Serial.println("DIRECT_SKIP tunnel_not_connected");
        logDirectResult("skip", 0);
        finishDirectCycle("direct_skip_after", before);
        return;
    }

    const char* requestPath = directRequestPath();
    Serial.print("DIRECT_OPEN ");
    Serial.print(VPN_DIRECT_TEST_HOST);
    Serial.print(":");
    Serial.println(VPN_DIRECT_TEST_PORT);
    Serial.print("DIRECT_PATH ");
    Serial.println(requestPath);

    SSHDirectClient client;
    if (!tunnel.openDirectTcpip(client,
                                VPN_DIRECT_TEST_HOST,
                                VPN_DIRECT_TEST_PORT,
                                "127.0.0.1",
                                0,
                                5000)) {
        Serial.println("DIRECT_OPEN_FAIL");
        logDirectResult("open_fail", 0);
        finishDirectCycle("direct_open_fail_after", before);
        return;
    }
    logMemorySnapshot("direct_opened");

    char request[256];
    int requestLen = snprintf(request, sizeof(request),
                              "GET %s HTTP/1.1\r\nHost: %s\r\n"
                              "Connection: close\r\n\r\n",
                              requestPath, VPN_DIRECT_TEST_HOST);
    if (requestLen < 0 || requestLen >= static_cast<int>(sizeof(request))) {
        Serial.println("DIRECT_REQUEST_TOO_LONG");
        client.stop(10000);
        logDirectResult("request_too_long", 0);
        finishDirectCycle("direct_request_fail_after", before);
        return;
    }

    size_t sent = client.write(reinterpret_cast<const uint8_t*>(request),
                               static_cast<size_t>(requestLen),
                               5000);
    Serial.print("DIRECT_SENT ");
    Serial.println(sent);

    uint8_t buffer[513];
    bool gotAny = false;
    size_t receivedTotal = 0;
    uint32_t deadline = millis() + 8000UL;
    while (static_cast<int32_t>(millis() - deadline) < 0) {
        reverseServer.handleClient();
        tunnel.loop();

        int rc = client.read(buffer, sizeof(buffer) - 1, 500);
        if (rc > 0) {
            gotAny = true;
            receivedTotal += static_cast<size_t>(rc);
            buffer[rc] = '\0';
            Serial.write(buffer, rc);
            deadline = millis() + 1000UL;
            continue;
        }
        if (rc < 0) {
            Serial.print("\nDIRECT_READ_FAIL ");
            Serial.println(client.lastError());
            break;
        }
        if (gotAny && !client.connected()) {
            break;
        }
    }
    Serial.println();
    Serial.print(gotAny ? "DIRECT_DONE bytes=" : "DIRECT_NO_RESPONSE bytes=");
    Serial.println(receivedTotal);
    logDirectResult(gotAny ? "success" : "no_response", receivedTotal);
    bool stopped = client.stop(10000);
    Serial.print("DIRECT_STOP ");
    Serial.println(stopped ? "OK" : "FAIL");
    Serial.printf("DIRECT_STOP_STATS ms=%lu close_eagain=%u free_eagain=%u "
                  "close_rc=%d free_rc=%d last_error=%d\n",
                  static_cast<unsigned long>(client.lastStopDurationMs()),
                  client.lastStopCloseEagainCount(),
                  client.lastStopFreeEagainCount(),
                  client.lastStopCloseResult(),
                  client.lastStopFreeResult(),
                  client.lastError());
    finishDirectCycle("direct_after", before);
}
} // namespace

void setup()
{
    Serial.begin(9600);
    delay(5000);
    Serial.println();
    Serial.println("SSH_DUPLEX_TUNNEL_STUB");

    if (!connectWiFi()) {
        return;
    }

    reverseServer.on("/", HTTP_GET, handleReverseRoot);
    reverseServer.onNotFound(handleReverseRoot);
    reverseServer.begin();
    Serial.println("REVERSE_LOCAL_HTTP_OK");
    logMemorySnapshot("reverse_server_started");

    if (connectSsh()) {
        runDirectHttpTest();
        nextDirectTestMs = millis() + VPN_DIRECT_TEST_INTERVAL_MS;
        nextMemoryLogMs = millis() + VPN_MEMORY_LOG_INTERVAL_MS;
    }
}

void loop()
{
    reverseServer.handleClient();
    tunnel.loop();

    if (WiFi.status() != WL_CONNECTED) {
        delay(250);
        return;
    }

    if (tunnel.getState() == TUNNEL_ERROR) {
        delay(50);
        return;
    }

    uint32_t now = millis();
    if (nextDirectTestMs != 0 &&
        static_cast<int32_t>(now - nextDirectTestMs) >= 0) {
        runDirectHttpTest();
        nextDirectTestMs = now + VPN_DIRECT_TEST_INTERVAL_MS;
    }

    if (nextMemoryLogMs != 0 &&
        static_cast<int32_t>(now - nextMemoryLogMs) >= 0) {
        logMemorySnapshot("periodic");
        nextMemoryLogMs = now + VPN_MEMORY_LOG_INTERVAL_MS;
    }

    delay(5);
}
