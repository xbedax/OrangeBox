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

bool timeReached(uint32_t now, uint32_t timestamp)
{
    return timestamp == 0 || static_cast<int32_t>(now - timestamp) >= 0;
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
    nextConnectAttemptMs = 0;

    if (!hasBackend()) {
        configured = false;
        state = VPNConnectionState::BackendUnavailable;
        return false;
    }

    if (!isConfigValid(config)) {
        configured = false;
        state = VPNConnectionState::ConfigInvalid;
        return false;
    }

    configured = applyConfig(config);
    state = configured ? VPNConnectionState::Disconnected : VPNConnectionState::ConfigInvalid;
    return configured;
}

bool VPNManager::connect()
{
    if (!configured) {
        return false;
    }

#if VPN_MANAGER_HAS_REVERSE_TUNNEL
    uint32_t now = millis();
    if (!canAttemptConnect(now)) {
        return false;
    }

    auto* tunnel = static_cast<SSHTunnel*>(client);
    if (tunnel == nullptr) {
        state = VPNConnectionState::BackendUnavailable;
        return false;
    }
    if (tunnel->isConnected()) {
        state = VPNConnectionState::Connected;
        nextConnectAttemptMs = 0;
        return true;
    }
    if (!initialized) {
        initialized = tunnel->init();
        if (!initialized) {
            scheduleReconnect(VPNConnectionState::Error, config.reconnectDelayMs);
            return false;
        }
    }

    state = VPNConnectionState::Connecting;
    if (tunnel->connectSSH()) {
        state = VPNConnectionState::Connected;
        nextConnectAttemptMs = 0;
        return true;
    }

    if (backendLastFailureWasStaleListener()) {
        scheduleReconnect(VPNConnectionState::StaleRemoteListener,
                          config.staleRemoteListenerCooldownMs);
    } else {
        scheduleReconnect(VPNConnectionState::Error, config.reconnectDelayMs);
    }
    return false;
#else
    state = VPNConnectionState::BackendUnavailable;
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
    state = configured ? VPNConnectionState::Disconnected : VPNConnectionState::Unconfigured;
    nextConnectAttemptMs = 0;
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
    if (!configured) {
        return;
    }

    auto* tunnel = static_cast<SSHTunnel*>(client);
    if (tunnel == nullptr) {
        state = VPNConnectionState::BackendUnavailable;
        return;
    }

    uint32_t now = millis();
    if (!initialized || state == VPNConnectionState::Disconnected ||
        state == VPNConnectionState::Error ||
        state == VPNConnectionState::StaleRemoteListener) {
        if (canAttemptConnect(now)) {
            connect();
        }
        return;
    }

    if (state == VPNConnectionState::Connected || tunnel->isConnected()) {
        tunnel->loop();
        if (tunnel->isConnected()) {
            state = VPNConnectionState::Connected;
            return;
        }

        if (backendLastFailureWasStaleListener()) {
            scheduleReconnect(VPNConnectionState::StaleRemoteListener,
                              config.staleRemoteListenerCooldownMs);
        } else {
            scheduleReconnect(VPNConnectionState::Error, config.reconnectDelayMs);
        }
    }
#endif
}

VPNConnectionState VPNManager::getState() const
{
    return state;
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
    switch (state) {
    case VPNConnectionState::Unconfigured:
        return String("Unconfigured");
    case VPNConnectionState::Disconnected:
        return String("Disconnected");
    case VPNConnectionState::Connecting:
        return String("Connecting");
    case VPNConnectionState::Connected:
        return String("Connected");
    case VPNConnectionState::StaleRemoteListener:
        return String("StaleRemoteListener");
    case VPNConnectionState::Error:
        return String("Error");
    case VPNConnectionState::BackendUnavailable:
        return String("BackendUnavailable");
    case VPNConnectionState::ConfigInvalid:
        return String("ConfigInvalid");
    default:
        return String("Unknown");
    }
}

uint32_t VPNManager::getNextReconnectDelayMs() const
{
    if (nextConnectAttemptMs == 0) {
        return 0;
    }

    uint32_t now = millis();
    if (timeReached(now, nextConnectAttemptMs)) {
        return 0;
    }
    return nextConnectAttemptMs - now;
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
        || vpnConfig.staleRemoteListenerCooldownMs == 0
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

bool VPNManager::canAttemptConnect(uint32_t now) const
{
    return timeReached(now, nextConnectAttemptMs);
}

bool VPNManager::backendLastFailureWasStaleListener() const
{
#if VPN_MANAGER_HAS_REVERSE_TUNNEL
    auto* tunnel = static_cast<SSHTunnel*>(client);
    return tunnel != nullptr && tunnel->lastConnectFailureWasReverseListener();
#else
    return false;
#endif
}

void VPNManager::scheduleReconnect(VPNConnectionState failureState, uint32_t delayMs)
{
    state = failureState;
    nextConnectAttemptMs = millis() + delayMs;
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
