#include "logger.h"
#include "log_serialization.h"
#include <ArduinoJson.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

class TestTransport : public BufferedLogTransport {
public:
    bool connected = true;
    bool fail = false;
    std::vector<QueuedLogRecord> delivered;
    bool isConnected() const override { return connected; }
    bool reconnect(unsigned long) override { return connected; }
    bool sendNow(const QueuedLogRecord& record) override {
        if (fail) return false;
        delivered.push_back(record);
        return true;
    }
};

static LogParam param(const QueuedLogRecord& record, uint8_t index) {
    LogParam result;
    assert(record.params.get(index, result));
    return result;
}

int main() {
    static_assert(LOG_MAX_PARAMS == 12, "The approved firmware limit is twelve");
    TestTransport transport;
    Logger sender;
    sender.begin(&transport, "BOX-007");
    sender.logPrint(A_PASSWORD_STARTED, LOGAREA_OPERATION, SEVERITY_DEBUG);
    sender.logPrint(A_PASSWORD_STARTED, LOGAREA_OPERATION, SEVERITY_INFO);
    sender.logPrint(A_PASSWORD_STARTED, LOGAREA_OPERATION, SEVERITY_WARNING);
    sender.logPrint(A_PASSWORD_STARTED, LOGAREA_OPERATION, SEVERITY_ERROR);
    sender.logPrint(A_PASSWORD_STARTED, LOGAREA_OPERATION, SEVERITY_CRITICAL);
    assert(transport.queuedCount() == 6 - LOGPRINT_MIN_SEVERITY);
    sender.update(1);
    assert(transport.delivered.size() == 6 - LOGPRINT_MIN_SEVERITY);
    assert(transport.delivered.front().severity == LOGPRINT_MIN_SEVERITY);
    assert(transport.delivered.back().severity == SEVERITY_CRITICAL);
    transport.begin();
    transport.delivered.clear();

    // Both String temporaries and mutable strings must survive until delivery.
    {
        char ip[] = "192.168.1.20";
        sender.logPrint(A_CLIENT_CONNECTED, LOGAREA_COMM, SEVERITY_CRITICAL, 7u, ip);
        memset(ip, 'X', strlen(ip));
    }
    sender.logPrint(A_CLIENT_CONNECTED, LOGAREA_COMM, SEVERITY_CRITICAL, 8u, String("001234"));
    sender.update(2);
    assert(transport.delivered.size() == 2);
    const auto first = transport.delivered[0];
    assert(first.message == A_CLIENT_CONNECTED && strcmp(first.source, "BOX-007") == 0);
    assert(param(first, 0).type == LogParam::Type::Unsigned);
    assert(param(first, 0).value.unsignedValue == 7);
    assert(strcmp(param(first, 1).value.textValue, "192.168.1.20") == 0);
    assert(strcmp(param(transport.delivered[1], 1).value.textValue, "001234") == 0);
    char text[768];
    assert(formatLogMessage(first.message, first.params, false, text, sizeof(text)));
    assert(strcmp(text, "WebSocket client #7 connected from 192.168.1.20") == 0);
    assert(formatLogMessage(first.message, first.params, true, text, sizeof(text)));
    assert(strcmp(text, "Client 7 online") == 0);
    assert(!formatLogMessage(first.message, first.params, false, text, 8));
    assert(text[7] == '\0');

    // JSON has typed values and named parameters, independent of key order.
    char json[1536]; size_t written;
    assert(serializeLogRecord(first, json, sizeof(json), written));
    JsonDocument doc;
    assert(!deserializeJson(doc, json));
    assert(doc["data"]["message"].is<uint16_t>());
    assert(doc["data"]["message"] == A_CLIENT_CONNECTED);
    assert(doc["data"]["messageparams"]["p1"] == 7);
    assert(doc["data"]["messageparams"]["p2"] == "192.168.1.20");
    assert(doc["data"]["timestamp"] == doc["_timestamp_"]);
    assert(doc["_command_"] == "c_log");
    assert(!serializeLogRecord(first, json, 8, written));
    JsonDocument shuffled;
    assert(!deserializeJson(shuffled, "{\"p2\":\"192.168.1.20\",\"p1\":7}"));
    assert(shuffled["p1"] == doc["data"]["messageparams"]["p1"]);
    assert(shuffled["p2"] == doc["data"]["messageparams"]["p2"]);

    // A copied block contains no internal pointers. Preserve all numeric types.
    QueuedLogRecord mixed = {};
    mixed.message = 65000; // Deliberately absent ID exercises the fallback.
    assert(mixed.params.append(LogParam(std::numeric_limits<int64_t>::min())));
    assert(mixed.params.append(LogParam(std::numeric_limits<uint64_t>::max())));
    assert(mixed.params.append(LogParam(1.25)));
    assert(mixed.params.append(LogParam(true)));
    assert(mixed.params.append(LogParam("001234")));
    auto copied = mixed;
    memset(mixed.params.data, 0, sizeof(mixed.params.data));
    assert(serializeLogRecord(copied, json, sizeof(json), written));
    assert(!deserializeJson(doc, json));
    assert(doc["data"]["messageparams"]["p1"].as<int64_t>() == std::numeric_limits<int64_t>::min());
    assert(doc["data"]["messageparams"]["p2"].as<uint64_t>() == std::numeric_limits<uint64_t>::max());
    assert(doc["data"]["messageparams"]["p3"].as<double>() == 1.25);
    assert(doc["data"]["messageparams"]["p4"].is<bool>());
    assert(doc["data"]["messageparams"]["p4"].as<bool>());
    assert(doc["data"]["messageparams"]["p5"] == "001234");
    assert(formatLogMessage(copied.message, copied.params, false, text, sizeof(text)));
    assert(strstr(text, "A65000") && strstr(text, "p5=001234"));
    copied.params.data[0] = 255;
    assert(!serializeLogRecord(copied, json, sizeof(json), written));

    LogParamBlock full;
    for (uint8_t i = 0; i < LOG_MAX_PARAMS; ++i) assert(full.append(LogParam(uint64_t(i))));
    assert(!full.append(LogParam(13u)));
    assert(!LogParamBlock().append(LogParam(std::numeric_limits<double>::infinity())));
    LogParamBlock textBlock;
    const std::string longest(LOG_PARAM_DATA_MAX_LEN - 4, '\x01');
    assert(textBlock.append(LogParam(longest.c_str())));
    assert(textBlock.used == LOG_PARAM_DATA_MAX_LEN);
    auto escaped = first;
    escaped.params = textBlock;
    assert(serializeLogRecord(escaped, json, sizeof(json), written));
    assert(written > 512); // Escaping must fit the larger WebSocket frame.
    assert(strstr(json, "\\u0001") && !strchr(json, '\x01'));
    assert(!deserializeJson(doc, json));
    assert(doc["data"]["messageparams"]["p1"].as<std::string>() == longest);

    // Retry keeps the head: no reordering and no loss after a send failure.
    transport.begin(); transport.delivered.clear();
    sender.logPrint(A_DOOR_OPENED, LOGAREA_OPERATION, SEVERITY_CRITICAL, 1u);
    sender.logPrint(A_DOOR_OPENED, LOGAREA_OPERATION, SEVERITY_CRITICAL, 2u);
    transport.fail = true;
    sender.update(3);
    assert(transport.queuedCount() == 2);
    transport.fail = false;
    sender.update(4);
    assert(param(transport.delivered[0], 0).value.unsignedValue == 1);
    assert(param(transport.delivered[1], 0).value.unsignedValue == 2);

    // Overflow replaces the oldest event and emits one structured error notice.
    transport.begin(); transport.delivered.clear();
    for (unsigned i = 0; i < LOG_QUEUE_SIZE + 2; ++i)
        sender.logPrint(A_DOOR_OPENED, LOGAREA_OPERATION, SEVERITY_CRITICAL, i);
    assert(transport.queuedCount() == LOG_QUEUE_SIZE && transport.droppedCount() == 2);
    sender.update(5);
    assert(transport.delivered[0].message == A_LOG_DROPPED);
    assert(transport.delivered[0].severity == SEVERITY_ERROR);
    assert(param(transport.delivered[0], 0).value.unsignedValue == 2);
    assert(param(transport.delivered[1], 0).value.unsignedValue == 2);
    assert(transport.droppedCount() == 0 && transport.queuedCount() == 0);

    // Oversized text / wrong parameter count produce an error, never truncation.
    transport.begin(); transport.delivered.clear();
    std::string oversized(LOG_PARAM_DATA_MAX_LEN, 'x');
    sender.logPrint(A_CLIENT_CONNECTED, LOGAREA_COMM, SEVERITY_CRITICAL, 7u, oversized.c_str());
    sender.logPrint(A_CLIENT_CONNECTED, LOGAREA_COMM, SEVERITY_CRITICAL, 7u);
    assert(sender.rejectedCount() == 2);
    sender.update(6);
    assert(transport.delivered.size() == 2);
    for (const auto& event : transport.delivered) assert(event.message == A_LOG_REJECTED);

    transport.begin(); transport.delivered.clear();
    sender.logPrint(A_DIAGNOSTIC_SNAPSHOT, LOGAREA_SYSTEM, SEVERITY_CRITICAL,
                    1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u);
    sender.update(7);
    assert(transport.delivered.size() == 1 && transport.delivered[0].params.count == 11);
    assert(serializeLogRecord(transport.delivered[0], json, sizeof(json), written));
    assert(!deserializeJson(doc, json));
    assert(doc["data"]["messageparams"].size() == 11);
    assert(doc["data"]["messageparams"]["p11"] == 11);
    assert(sender.rejectedCount() == 2);
    std::cout << "Logging tests passed (minimum severity " << LOGPRINT_MIN_SEVERITY << ")\n";
}
