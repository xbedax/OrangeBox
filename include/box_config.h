#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "gpio_hal.h"

// Runtime copy of the selected box_setup.h branch. JSON uses the same names.
struct BoxConfig {
    BoxConfig();
    bool provisioned;
    bool scannerTestKeepCred;
    int language;
    String hostName, wifiSsid, wifiPassword;
    String logHost, logPath, ntpServer;
    uint16_t logPort;
    long gmtOffset, daylightOffset;
    String vpnHost, vpnUser, vpnKeyType, vpnFingerprint, vpnPublicKey, vpnPrivateKey;
    uint16_t vpnPort, vpnRemotePort;
    uint8_t ambientPin, doorCount;
    DoorMapping doors[DOOR_COUNT] = {};
};

class BoxConfigStore {
public:
    static constexpr size_t MaxBytes = 32768;
    static constexpr const char* Path = "/box_config.json";
    static constexpr const char* PendingPath = "/box_config.pending";
    bool begin();
    bool load(BoxConfig& config, String& error);
    // Validate first; a failed update never changes the active configuration.
    bool decode(JsonVariantConst root, BoxConfig& config, String& error, bool complete = false);
    bool commitDownloaded(String& error);
private:
    bool mounted = false;
};
