#ifndef VPN_MANAGER_H
#define VPN_MANAGER_H

#include <Arduino.h>

#ifndef VPN_GATEWAY_HOST
#define VPN_GATEWAY_HOST "192.168.0.1"
#endif
#ifndef VPN_GATEWAY_PORT
#define VPN_GATEWAY_PORT 8080
#endif
#ifndef VPN_REMOTE_BIND_PORT
#define VPN_REMOTE_BIND_PORT 8085
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

    uint16_t keepAliveIntervalSec = 30;

    uint32_t reconnectDelayMs = 5000;
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

    int getBoundPort() const;
    String getStateString() const;

private:
    void* client = nullptr;
    VPNConfig config;
    bool configured = false;
    bool initialized = false;

    bool hasBackend() const;
    bool isConfigValid(const VPNConfig& vpnConfig) const;
    bool applyConfig(const VPNConfig& vpnConfig);
};

extern VPNManager vpnManager;

#endif // VPN_MANAGER_H
