#pragma once
#include <Arduino.h>

struct ProvisioningPayload {
    String id, name, ssid, password, host, nonce;
    uint16_t port = 0;
    static bool parse(const uint8_t* data, size_t length, ProvisioningPayload& out);
    String tokenInput() const { return nonce + "|" + id + "|" + name; }
};
