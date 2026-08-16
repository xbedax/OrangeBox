#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "vpn_manager.h"

namespace {
const char* wifiSsid = WIFI_SSID;
const char* wifiPassword = WIFI_PASSWD;

const char* sshHost = VPN_GATEWAY_HOST ;
const uint16_t sshPort = VPN_GATEWAY_PORT;
const char* sshUser = VPN_USERNAME;

const char* sshPrivateKey = VPN_PRIVATE_KEY;

const char* sshPublicKey = VPN_PUBLIC_KEY;

const char* remoteBindHost = "127.0.0.1";
const uint16_t remoteHttpPort = VPN_REMOTE_BIND_PORT;
const uint16_t localHttpPort = 80;

WebServer helloServer(localHttpPort);
char localHost[16] = "127.0.0.1";
unsigned long nextStatusMillis = 0;

void setLocalHostFromWiFi()
{
    IPAddress ip = WiFi.localIP();
    snprintf(localHost, sizeof(localHost), "%u.%u.%u.%u",
             ip[0], ip[1], ip[2], ip[3]);
}

void handleRoot()
{
    helloServer.send(200, "text/plain", "hello world\n");
}

bool connectWiFi()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSsid, wifiPassword);

    Serial.print("Connecting WiFi");
    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print('.');
        attempts++;
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WIFI_FAIL");
        return false;
    }

    setLocalHostFromWiFi();
    Serial.printf("WIFI_OK ip=%s rssi=%d\n", localHost, WiFi.RSSI());
    return true;
}

bool startTunnel()
{
    VPNConfig vpnConfig;
    vpnConfig.sshHost = sshHost;
    vpnConfig.sshPort = sshPort;
    vpnConfig.username = sshUser;
    vpnConfig.authMode = VPNAuthMode::PublicKey;
    vpnConfig.privateKey = sshPrivateKey;
    vpnConfig.publicKey = sshPublicKey;
    vpnConfig.keyPassphrase = "";
    vpnConfig.verifyHostKey = false;
    vpnConfig.tunnel.remoteBindHost = remoteBindHost;
    vpnConfig.tunnel.remoteBindPort = remoteHttpPort;
    vpnConfig.tunnel.localHost = localHost;
    vpnConfig.tunnel.localPort = localHttpPort;
    vpnConfig.keepAliveIntervalSec = 30;
    vpnConfig.reconnectDelayMs = 5000;
    vpnConfig.maxReconnectAttempts = 10;
    vpnConfig.connectionTimeoutSec = 30;
    vpnConfig.bufferSize = 8192;
    vpnConfig.maxChannels = 3;
    vpnConfig.debugEnabled = true;

    if (!vpnManager.begin(vpnConfig)) {
        Serial.println("VPN_BEGIN_FAIL");
        return false;
    } else {
        Serial.println("VPN_BEGIN_OK");
    }
    if (!vpnManager.connect()) {
        Serial.println("VPN_CONNECT_FAIL");
        return false;
    } else {
        Serial.println("VPN_CONNECT_OK");
    }

    Serial.printf("VPN_CONNECTED remote=%s:%u local=%s:%u bound=%d\n",
                  remoteBindHost, remoteHttpPort, localHost, localHttpPort,
                  vpnManager.getBoundPort());
    Serial.printf("Test from SSH server: curl http://%s:%u/\n",
                  remoteBindHost, remoteHttpPort);
    return true;
}
}

void setup()
{
    Serial.begin(9600);
    delay(1000);
    Serial.println("VPN reverse tunnel hello-world stub");

    if (!connectWiFi()) {
        return;
    }

    helloServer.on("/", handleRoot);
    helloServer.begin();
    Serial.printf("HTTP_OK local=http://%s:%u/\n", localHost, localHttpPort);

    startTunnel();
}

void loop()
{
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
        delay(500);
        return;
    }

    helloServer.handleClient();
    vpnManager.update();

    unsigned long now = millis();
    if (now >= nextStatusMillis) {
        nextStatusMillis = now + 10000;
        Serial.printf("VPN_STATE connected=%u state=%s bound=%d\n",
                      vpnManager.isConnected() ? 1 : 0,
                      vpnManager.getStateString().c_str(),
                      vpnManager.getBoundPort());
    }

    delay(1);
}
