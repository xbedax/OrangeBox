#ifndef VPN_MANAGER_H
#define VPN_MANAGER_H

#include <Arduino.h>

#ifndef VPN_GATEWAY_HOST
#define VPN_GATEWAY_HOST "192.168.0.1"
#endif
#ifndef VPN_GATEWAY_PORT
#define VPN_GATEWAY_PORT 8080
#endif
// Fallback only. Project config/credentials should set this per box instance.
#ifndef VPN_REMOTE_BIND_PORT
#define VPN_REMOTE_BIND_PORT 8084
#endif
#ifndef VPN_SERVER_CLIENT_ALIVE_INTERVAL_SEC
#define VPN_SERVER_CLIENT_ALIVE_INTERVAL_SEC 7
#endif
#ifndef VPN_SERVER_CLIENT_ALIVE_COUNT_MAX
#define VPN_SERVER_CLIENT_ALIVE_COUNT_MAX 3
#endif
#ifndef VPN_SERVER_STALE_SESSION_WINDOW_SEC
#define VPN_SERVER_STALE_SESSION_WINDOW_SEC \
    (VPN_SERVER_CLIENT_ALIVE_INTERVAL_SEC * VPN_SERVER_CLIENT_ALIVE_COUNT_MAX)
#endif
#ifndef VPN_CLIENT_KEEPALIVE_MARGIN_SEC
#define VPN_CLIENT_KEEPALIVE_MARGIN_SEC 9
#endif
#ifndef VPN_CLIENT_KEEPALIVE_INTERVAL_SEC
#define VPN_CLIENT_KEEPALIVE_INTERVAL_SEC \
    (VPN_SERVER_STALE_SESSION_WINDOW_SEC + VPN_CLIENT_KEEPALIVE_MARGIN_SEC)
#endif
#ifndef VPN_STALE_REMOTE_LISTENER_GRACE_SEC
#define VPN_STALE_REMOTE_LISTENER_GRACE_SEC 14
#endif
#ifndef VPN_STALE_REMOTE_LISTENER_COOLDOWN_MS
#define VPN_STALE_REMOTE_LISTENER_COOLDOWN_MS \
    ((VPN_SERVER_STALE_SESSION_WINDOW_SEC + VPN_STALE_REMOTE_LISTENER_GRACE_SEC) * 1000UL)
#endif
#ifndef VPN_USERNAME
#define VPN_USERNAME "some_name"
#endif
#ifndef VPN_GATEWAY_FINGERPRINT
#define VPN_GATEWAY_FINGERPRINT "SHA256:some_fingerprint" // root@webproxy
#endif
#ifndef VPN_GATEWAY_KEY_TYPE
#define VPN_GATEWAY_KEY_TYPE "your_type"
#endif
#ifndef VPN_PRIVATE_KEY
#define VPN_PRIVATE_KEY " -----BEGIN OPENSSH PRIVATE KEY----- blahblah= -----END OPENSSH PRIVATE KEY-----"
#endif
#ifndef VPN_PUBLIC_KEY
#define VPN_PUBLIC_KEY "your_type some_pub_key"
#endif

enum class VPNAuthMode : uint8_t {
    Password,
    PublicKey
};

enum class VPNConnectionState : uint8_t {
    Unconfigured,
    Disconnected,
    Connecting,
    Connected,
    StaleRemoteListener,
    Error,
    BackendUnavailable,
    ConfigInvalid
};

struct VPNTunnelMapping {
    const char* remoteBindHost = "127.0.0.1";
    uint16_t remoteBindPort = VPN_REMOTE_BIND_PORT;
    const char* localHost = "127.0.0.1";
    uint16_t localPort = 80;
};

struct VPNConfig {
    const char* sshHost = nullptr;
    uint16_t sshPort = VPN_GATEWAY_PORT;
    const char* username = nullptr;

    VPNAuthMode authMode = VPNAuthMode::PublicKey;
    const char* password = nullptr;
//    const char* privateKey = " -----BEGIN OPENSSH PRIVATE KEY----- b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAMwAAAAtzc2gtZW QyNTUxOQAAACCA7O0VJrR0HD08WeAvwTDTKh/u8aThnrpBTMGS4+JFtgAAAJACdvMsAnbz LAAAAAtzc2gtZWQyNTUxOQAAACCA7O0VJrR0HD08WeAvwTDTKh/u8aThnrpBTMGS4+JFtg AAAEBbLXtjVSZwwu0/vrXXDz0T+OwZf5/RL3BLWKjF9Xs1XoDs7RUmtHQcPTxZ4C/BMNMq H+7xpOGeukFMwZLj4kW2AAAADXJvb3RAd2VicHJveHk= -----END OPENSSH PRIVATE KEY-----";
    const char* privateKey = VPN_PRIVATE_KEY;
    const char* publicKey = VPN_PUBLIC_KEY;
    const char* keyPassphrase = "";

    bool verifyHostKey = true;
    const char* hostKeyFingerprint = VPN_GATEWAY_FINGERPRINT;
    const char* hostKeyType = VPN_GATEWAY_KEY_TYPE;

    VPNTunnelMapping tunnel;

    uint16_t keepAliveIntervalSec = VPN_CLIENT_KEEPALIVE_INTERVAL_SEC;

    uint32_t reconnectDelayMs = 5000;
    uint32_t staleRemoteListenerCooldownMs = VPN_STALE_REMOTE_LISTENER_COOLDOWN_MS;
    uint8_t maxReconnectAttempts = 5;
    uint16_t connectionTimeoutSec = 30;
    uint16_t bufferSize = 8192;
    uint8_t maxChannels = 5;
    uint32_t channelTimeoutMs = 1800000UL;
    size_t tunnelRingBufferSize = 64 * 1024;

    bool debugEnabled = false;
    uint32_t debugBaudRate = 115200;
};

class VPNManager {
public:
    VPNManager();

    bool begin(const VPNConfig& vpnConfig);
    bool connect();
    bool disconnect();
    bool isConnected() const;
    bool reconnect();
    void update();

    VPNConnectionState getState() const;
    int getBoundPort() const;
    String getStateString() const;
    uint32_t getNextReconnectDelayMs() const;

private:
    void* client = nullptr;
    VPNConfig config;
    bool configured = false;
    bool initialized = false;
    VPNConnectionState state = VPNConnectionState::Unconfigured;
    uint32_t nextConnectAttemptMs = 0;

    bool hasBackend() const;
    bool isConfigValid(const VPNConfig& vpnConfig) const;
    bool applyConfig(const VPNConfig& vpnConfig);
    bool canAttemptConnect(uint32_t now) const;
    bool backendLastFailureWasStaleListener() const;
    void scheduleReconnect(VPNConnectionState failureState, uint32_t delayMs);
};

extern VPNManager vpnManager;

#endif // VPN_MANAGER_H
