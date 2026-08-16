#include "vpn_manager.h"

#if __has_include(<ESP-Reverse_Tunneling_Libssh2.h>)
#include <ESP-Reverse_Tunneling_Libssh2.h>
#define VPN_MANAGER_HAS_REVERSE_TUNNEL 1
#elif __has_include("ESP-Reverse_Tunneling_Libssh2.h")
#include "ESP-Reverse_Tunneling_Libssh2.h"
#define VPN_MANAGER_HAS_REVERSE_TUNNEL 1
#else
#define VPN_MANAGER_HAS_REVERSE_TUNNEL 0
#endif

#if VPN_MANAGER_HAS_REVERSE_TUNNEL
static SSHTunnel sshTunnelClient;
#endif

VPNManager vpnManager;

namespace {
bool isTextSet(const char* value)
{
    return value != nullptr && value[0] != '\0';
}

const char* textOrEmpty(const char* value)
{
    return value != nullptr ? value : "";
}

bool isPortSet(uint16_t port)
{
    return port != 0;
}
}

VPNManager::VPNManager()
    : client(
#if VPN_MANAGER_HAS_REVERSE_TUNNEL
          &sshTunnelClient
#else
          nullptr
#endif
      )
{
}

bool VPNManager::begin(const VPNConfig& vpnConfig)
{
    disconnect();

    config = vpnConfig;
    initialized = false;
    configured = hasBackend() && isConfigValid(config) && applyConfig(config);
    return configured;
}

bool VPNManager::connect()
{
    if (!configured) {
        return false;
    }

#if VPN_MANAGER_HAS_REVERSE_TUNNEL
    auto* tunnel = static_cast<SSHTunnel*>(client);
    if (tunnel == nullptr) {
        return false;
    }
    if (tunnel->isConnected()) {
        return true;
    }
    if (!initialized) {
        initialized = tunnel->init();
        if (!initialized) {
            return false;
        }
    }

    return tunnel->connectSSH();
#else
    return false;
#endif
}

bool VPNManager::disconnect()
{
#if VPN_MANAGER_HAS_REVERSE_TUNNEL
    auto* tunnel = static_cast<SSHTunnel*>(client);
    if (tunnel != nullptr) {
        tunnel->disconnect();
    }
#endif
    return !isConnected();
}

bool VPNManager::isConnected() const
{
#if VPN_MANAGER_HAS_REVERSE_TUNNEL
    auto* tunnel = static_cast<SSHTunnel*>(client);
    return tunnel != nullptr && tunnel->isConnected();
#else
    return false;
#endif
}

bool VPNManager::reconnect()
{
    disconnect();
    return connect();
}

void VPNManager::update()
{
#if VPN_MANAGER_HAS_REVERSE_TUNNEL
    if (!initialized) {
        return;
    }

    auto* tunnel = static_cast<SSHTunnel*>(client);
    if (tunnel != nullptr) {
        tunnel->loop();
    }
#endif
}

int VPNManager::getBoundPort() const
{
#if VPN_MANAGER_HAS_REVERSE_TUNNEL
    auto* tunnel = static_cast<SSHTunnel*>(client);
    return tunnel != nullptr ? tunnel->getBoundPort() : 0;
#else
    return 0;
#endif
}

String VPNManager::getStateString() const
{
#if VPN_MANAGER_HAS_REVERSE_TUNNEL
    auto* tunnel = static_cast<SSHTunnel*>(client);
    return tunnel != nullptr ? tunnel->getStateString() : String("backend-unavailable");
#else
    return String("backend-unavailable");
#endif
}

bool VPNManager::hasBackend() const
{
#if VPN_MANAGER_HAS_REVERSE_TUNNEL
    return client != nullptr;
#else
    return false;
#endif
}

bool VPNManager::isConfigValid(const VPNConfig& vpnConfig) const
{
    if (!isTextSet(vpnConfig.sshHost)
        || !isPortSet(vpnConfig.sshPort)
        || !isTextSet(vpnConfig.username)
        || !isTextSet(vpnConfig.tunnel.remoteBindHost)
        || !isPortSet(vpnConfig.tunnel.remoteBindPort)
        || !isTextSet(vpnConfig.tunnel.localHost)
        || !isPortSet(vpnConfig.tunnel.localPort)
        || vpnConfig.keepAliveIntervalSec == 0
        || vpnConfig.reconnectDelayMs == 0
        || vpnConfig.maxReconnectAttempts == 0
        || vpnConfig.connectionTimeoutSec == 0
        || vpnConfig.bufferSize == 0
        || vpnConfig.maxChannels == 0
        || vpnConfig.tunnelRingBufferSize == 0) {
        return false;
    }
    if (vpnConfig.verifyHostKey && !isTextSet(vpnConfig.hostKeyFingerprint)) {
        return false;
    }

    if (vpnConfig.authMode == VPNAuthMode::Password) {
        return isTextSet(vpnConfig.password);
    }

    return isTextSet(vpnConfig.privateKey) && isTextSet(vpnConfig.publicKey);
}

bool VPNManager::applyConfig(const VPNConfig& vpnConfig)
{
#if VPN_MANAGER_HAS_REVERSE_TUNNEL
    if (vpnConfig.authMode == VPNAuthMode::Password) {
        globalSSHConfig.setSSHServer(vpnConfig.sshHost,
                                     vpnConfig.sshPort,
                                     vpnConfig.username,
                                     textOrEmpty(vpnConfig.password));
    } else {
        globalSSHConfig.setSSHKeyAuthFromMemory(vpnConfig.sshHost,
                                                vpnConfig.sshPort,
                                                vpnConfig.username,
                                                textOrEmpty(vpnConfig.privateKey),
                                                textOrEmpty(vpnConfig.publicKey),
                                                textOrEmpty(vpnConfig.keyPassphrase));
    }

    if (vpnConfig.verifyHostKey) {
        globalSSHConfig.setHostKeyVerification(textOrEmpty(vpnConfig.hostKeyFingerprint),
                                               textOrEmpty(vpnConfig.hostKeyType),
                                               true);
    } else {
        globalSSHConfig.setHostKeyVerification(false);
    }

    globalSSHConfig.clearTunnelMappings();
    globalSSHConfig.setMaxReverseListeners(1);
    globalSSHConfig.setTunnelConfig(vpnConfig.tunnel.remoteBindHost,
                                    vpnConfig.tunnel.remoteBindPort,
                                    vpnConfig.tunnel.localHost,
                                    vpnConfig.tunnel.localPort);
    globalSSHConfig.setConnectionConfig(vpnConfig.keepAliveIntervalSec,
                                        static_cast<int>(vpnConfig.reconnectDelayMs),
                                        vpnConfig.maxReconnectAttempts,
                                        vpnConfig.connectionTimeoutSec);
    globalSSHConfig.setBufferConfig(vpnConfig.bufferSize,
                                    vpnConfig.maxChannels,
                                    static_cast<int>(vpnConfig.channelTimeoutMs),
                                    vpnConfig.tunnelRingBufferSize);
    globalSSHConfig.setDebugConfig(vpnConfig.debugEnabled,
                                   static_cast<int>(vpnConfig.debugBaudRate));

    return globalSSHConfig.validateConfiguration();
#else
    (void)vpnConfig;
    return false;
#endif
}
