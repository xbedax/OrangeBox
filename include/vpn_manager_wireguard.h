#ifndef VPN_MANAGER_WIREGUARD_H
#define VPN_MANAGER_WIREGUARD_H

#include <Arduino.h>
#include <IPAddress.h>

// Rollback snapshot of the original WireGuard-based VPN manager API.
struct VPNConfig {
    IPAddress localIp = IPAddress(0, 0, 0, 0);
    const char* privateKey = nullptr;
    const char* endpointAddress = nullptr;
    const char* publicKey = nullptr;
    uint16_t endpointPort = 0;
    IPAddress subnet = IPAddress(255, 255, 255, 255);
    IPAddress gateway = IPAddress(0, 0, 0, 0);
    bool useNetworkConfig = false;
};

class VPNManager {
public:
    VPNManager();

    bool begin(const VPNConfig& vpnConfig);
    bool begin(const IPAddress& localIp,
               const char* privateKey,
               const char* endpointAddress,
               const char* publicKey,
               uint16_t endpointPort);
    bool begin(const IPAddress& localIp,
               const IPAddress& subnet,
               const IPAddress& gateway,
               const char* privateKey,
               const char* endpointAddress,
               const char* publicKey,
               uint16_t endpointPort);

    bool connect();
    bool disconnect();
    bool isConnected() const;
    bool reconnect();

private:
    void* client = nullptr;
    VPNConfig config;
    bool configured = false;

    bool hasBackend() const;
    bool isConfigValid(const VPNConfig& vpnConfig) const;
};

extern VPNManager vpnManager;

#endif // VPN_MANAGER_WIREGUARD_H
