#include "log_serialization.h"
#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>

bool serializeLogRecord(const QueuedLogRecord& record, char* output, size_t capacity, size_t& written) {
    written = 0;
    if (!output || !capacity || record.params.count > LOG_MAX_PARAMS) return false;
    JsonDocument doc;
    doc["_command_"] = COMM_LOG;
    doc["_timestamp_"] = record.timestamp;
    JsonObject data = doc["data"].to<JsonObject>();
    data["source"] = record.source;
    data["area"] = record.logArea;
    data["severity"] = record.severity;
    data["message"] = record.message;
    data["timestamp"] = record.timestamp;
    JsonObject params = data["messageparams"].to<JsonObject>();
    for (uint8_t i = 0; i < record.params.count; ++i) {
        LogParam param;
        if (!record.params.get(i, param)) return false;
        char key[8];
        snprintf(key, sizeof(key), "p%u", unsigned(i + 1));
        // String keys must be copied: key is reused on the next iteration.
        JsonVariant value = params[JsonString(key)].to<JsonVariant>();
        switch (param.type) {
            case LogParam::Type::Signed: value.set(param.value.signedValue); break;
            case LogParam::Type::Unsigned: value.set(param.value.unsignedValue); break;
            case LogParam::Type::Real: value.set(param.value.realValue); break;
            case LogParam::Type::Boolean: value.set(param.value.boolValue); break;
            case LogParam::Type::Text: value.set(param.value.textValue); break;
        }
    }
    if (doc.overflowed() || measureJson(doc) >= capacity) return false;
    const size_t serializedSize = serializeJson(doc, output, capacity);
    if (serializedSize != measureJson(doc)) return false;
    // ArduinoJson escapes common whitespace but may emit other C0 controls
    // literally. Expand those bytes in place to keep every output valid JSON.
    size_t controls = 0;
    for (size_t i = 0; i < serializedSize; ++i)
        if (static_cast<unsigned char>(output[i]) < 0x20) ++controls;
    const size_t expanded = serializedSize + controls * 5;
    if (expanded >= capacity) return false;
    size_t destination = expanded;
    output[destination] = '\0';
    const char hex[] = "0123456789abcdef";
    for (size_t i = serializedSize; i > 0;) {
        const unsigned char c = output[--i];
        if (c < 0x20) {
            destination -= 6;
            memcpy(output + destination, "\\u00", 4);
            output[destination + 4] = hex[c >> 4];
            output[destination + 5] = hex[c & 15];
        } else output[--destination] = c;
    }
    written = expanded;
    return true;
}
