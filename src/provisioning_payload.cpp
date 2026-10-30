#include "provisioning_payload.h"
#include <ArduinoJson.h>
#include <cstring>

bool ProvisioningPayload::parse(const uint8_t* data, size_t length, ProvisioningPayload& out) {
    if (!data || !length || length > 1024) return false;
    JsonDocument doc;
    if (deserializeJson(doc, data, length, DeserializationOption::NestingLimit(2))) return false;
    if (!doc.is<JsonObject>()) return false;
    ProvisioningPayload next;
    auto read = [&](const char* key, String& target, size_t max, bool empty = false) {
        if (!doc[key].is<const char*>()) return false;
        JsonString str = doc[key].as<JsonString>();
        if ((!empty && !str.size()) || str.size() > max || strlen(str.c_str()) != str.size()) return false;
        target = str.c_str(); return true;
    };
    if (!read("ID", next.id, 128) || !read("NAME", next.name, 63)
        || !read("W_SSID", next.ssid, 32) || !read("W_PASS", next.password, 63, true)
        || !read("SRV", next.host, 253) || !read("NONCE", next.nonce, 128)
        || !doc["PORT"].is<unsigned int>() || doc["PORT"].as<unsigned int>() == 0
        || doc["PORT"].as<unsigned int>() > 65535) return false;
    // SRV is a DNS hostname, never a scheme, path, userinfo or port.
    const char* host = next.host.c_str();
    size_t labelLength = 0;
    for (size_t i = 0; host[i]; ++i) {
        char c = host[i];
        if (c == '.') {
            if (!labelLength || host[i - 1] == '-') return false;
            labelLength = 0;
        } else {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '-')) return false;
            if ((!labelLength && c == '-') || ++labelLength > 63) return false;
        }
    }
    if (!labelLength || host[next.host.length() - 1] == '-') return false;
    if (strchr(next.id.c_str(), '|') || strchr(next.name.c_str(), '|') || strchr(next.nonce.c_str(), '|')) return false;
    next.port = doc["PORT"];
    out = next;
    return true;
}
