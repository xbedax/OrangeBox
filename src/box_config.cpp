#include "box_config.h"
#include "box_display.h"
#include "config.h"
#include <driver/gpio.h>
#include <cstring>

#ifndef LOG_SERVER_PATH
#define LOG_SERVER_PATH "/"
#endif

BoxConfig::BoxConfig()
    : provisioned(BOX_PROVISIONED != 0),
#ifdef QR_SCANNER_TEST_KEEP_CRED
      scannerTestKeepCred(true),
#else
      scannerTestKeepCred(false),
#endif
      language(DISP_LANG), hostName(BOX_HOST_NAME), wifiSsid(WIFI_SSID), wifiPassword(WIFI_PASSWD),
      logHost(LOG_SERVER_IP), logPath(LOG_SERVER_PATH), ntpServer(NTP_SERVER), logPort(LOG_SERVER_PORT),
      gmtOffset(GMT_OFFSET_SEC), daylightOffset(DAYLIGHT_OFFSET_SEC),
      vpnHost(VPN_GATEWAY_HOST), vpnUser(VPN_USERNAME), vpnKeyType(VPN_GATEWAY_KEY_TYPE),
      vpnFingerprint(VPN_GATEWAY_FINGERPRINT), vpnPublicKey(VPN_PUBLIC_KEY), vpnPrivateKey(VPN_PRIVATE_KEY),
      vpnPort(VPN_GATEWAY_PORT), vpnRemotePort(VPN_REMOTE_BIND_PORT), ambientPin(AMBIENT_PIN) {
    const DoorMapping defaults[] = INITIAL_DOOR_MAPPING;
    static_assert(sizeof(defaults) / sizeof(defaults[0]) <= DOOR_COUNT, "Too many default doors");
    static_assert(sizeof(defaults) / sizeof(defaults[0]) == ACTIVE_DOOR_NUMBERS, "Default door count mismatch");
    doorCount = sizeof(defaults) / sizeof(defaults[0]);
    memcpy(doors, defaults, sizeof(defaults));
}

bool BoxConfigStore::decode(JsonVariantConst root, BoxConfig& config, String& error, bool complete) {
    JsonObjectConst obj = root["sandbox"].as<JsonObjectConst>();
    if (obj.isNull()) { error = "Missing sandbox object"; return false; }
    BoxConfig next = config;
    auto text = [&](const char* key, String& target, size_t maxLen, bool empty = false) {
        if (obj[key].isUnbound()) { if (complete) error = key; return !complete; }
        if (!obj[key].is<const char*>()) { error = key; return false; }
        const char* value = obj[key];
        size_t len = strlen(value);
        // Reject embedded NUL, which otherwise silently truncates credentials.
        if (obj[key].as<JsonString>().size() != len || len > maxLen || (!empty && !len)) { error = key; return false; }
        target = value;
        return true;
    };
    auto number = [&](const char* key, long min, long max, long& result) {
        if (obj[key].isNull()) { if (!obj[key].isUnbound() || complete) { error = key; return false; } return true; }
        if (!obj[key].is<long>()) { error = key; return false; }
        long value = obj[key].as<long>();
        if (value < min || value > max) { error = key; return false; }
        result = value; return true;
    };
    auto boolean = [&](const char* key, bool& target) {
        if (obj[key].isUnbound()) { if (complete) error = key; return !complete; }
        if (obj[key].is<bool>()) { target = obj[key].as<bool>(); return true; }
        long value = target;
        if (!number(key, 0, 1, value)) return false;
        target = value; return true;
    };
    auto smallNumber = [&](const char* key, long min, long max, auto& target) {
        long value = target;
        if (!number(key, min, max, value)) return false;
        target = value; return true;
    };
    if (!boolean("BOX_PROVISIONED", next.provisioned)
        || !boolean("QR_SCANNER_TEST_KEEP_CRED", next.scannerTestKeepCred)
        || !smallNumber("DISP_LANG", 0, 1, next.language)
        || !text("BOX_HOST_NAME", next.hostName, 63)
        || !text("WIFI_SSID", next.wifiSsid, 32)
        || !text("WIFI_PASSWD", next.wifiPassword, 63, true)
        || !text("LOG_SERVER_IP", next.logHost, 253)
        || !text("LOG_SERVER_PATH", next.logPath, 256)
        || !smallNumber("LOG_SERVER_PORT", 1, 65535, next.logPort)
        || !text("NTP_SERVER", next.ntpServer, 253)
        || !number("GMT_OFFSET_SEC", -86400, 86400, next.gmtOffset)
        || !number("DAYLIGHT_OFFSET_SEC", -86400, 86400, next.daylightOffset)
        || !text("VPN_GATEWAY_HOST", next.vpnHost, 253)
        || !smallNumber("VPN_GATEWAY_PORT", 1, 65535, next.vpnPort)
        || !text("VPN_USERNAME", next.vpnUser, 128)
        || !smallNumber("VPN_REMOTE_BIND_PORT", 1, 65535, next.vpnRemotePort)
        || !text("VPN_GATEWAY_KEY_TYPE", next.vpnKeyType, 64)
        || !text("VPN_GATEWAY_FINGERPRINT", next.vpnFingerprint, 128)
        || !text("VPN_PUBLIC_KEY", next.vpnPublicKey, 4096)
        || !text("VPN_PRIVATE_KEY", next.vpnPrivateKey, 8192)
        || !smallNumber("AMBIENT_PIN", 0, 255, next.ambientPin)
        || !smallNumber("ACTIVE_DOOR_NUMBERS", 1, DOOR_COUNT, next.doorCount)) {
        if (error.length() == 0) error = "Missing configuration field";
        return false;
    }
    if (next.ambientPin != INVALID_PIN && !GPIO_IS_VALID_OUTPUT_GPIO(next.ambientPin)) {
        error = "AMBIENT_PIN"; return false;
    }
    if (!obj["INITIAL_DOOR_MAPPING"].isUnbound()) {
        JsonArrayConst mappings = obj["INITIAL_DOOR_MAPPING"].as<JsonArrayConst>();
        if (mappings.isNull() || mappings.size() != next.doorCount) { error = "INITIAL_DOOR_MAPPING count"; return false; }
        size_t i = 0;
        for (JsonArrayConst row : mappings) {
            if (row.size() != 5) { error = "INITIAL_DOOR_MAPPING row"; return false; }
            for (size_t c = 0; c < 5; ++c) {
                if (!row[c].is<int>() || row[c].as<int>() < 0 || row[c].as<int>() > (c == 4 ? 1 : 255)) {
                    error = "INITIAL_DOOR_MAPPING value"; return false;
                }
            }
            uint8_t lock = row[1], contact = row[2];
            // Extenders are not implemented by the current GPIO HAL.
            if (row[0].as<int>() == DOOR_UNKNOWN || row[3].as<int>() != 0
                || (lock != INVALID_PIN && !GPIO_IS_VALID_OUTPUT_GPIO(lock))
                || (contact != INVALID_PIN && !GPIO_IS_VALID_GPIO(contact))) {
                error = "INITIAL_DOOR_MAPPING GPIO"; return false;
            }
            next.doors[i++] = {row[0].as<uint8_t>(), lock, contact, 0, row[4].as<int>() != 0};
        }
    } else if (complete || next.doorCount != config.doorCount) {
        error = "INITIAL_DOOR_MAPPING missing"; return false;
    }
    if (complete && !next.provisioned) { error = "BOX_PROVISIONED must be 1"; return false; }
    config = next;
    return true;
}

