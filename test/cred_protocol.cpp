#include <cassert>
#include <cstring>
#include <iostream>
#include "cred_protocol.h"
#include "gpio_hal.h"
#include "logger.h"

Logger logger;
void Logger::logPrint(uint8_t, const String&, const char*, uint8_t) {}

// PinRecord layout before the API rename: existing CredentialV1 blobs must survive.
struct PreviousRecord {
    uint8_t recordType;
    uint8_t pinType;
    uint32_t pinId, prevId, nextId;
    uint8_t doorNum;
    char name[MAX_NAME_LENGTH + 1];
    union { char pin[MAX_PIN_LENGTH + 1]; char packetNumber[PACKET_NUMBER_MAX + 1]; };
    uint64_t validFrom, validTo;
    int32_t remaining;
};
static_assert(sizeof(PreviousRecord) == sizeof(CredRecord), "Existing NVS layout changed");
static_assert(offsetof(PreviousRecord, pinId) == offsetof(CredRecord, credId), "ID offset changed");
static_assert(offsetof(PreviousRecord, pinType) == offsetof(CredRecord, credType), "Type offset changed");
static_assert(offsetof(PreviousRecord, remaining) == offsetof(CredRecord, remaining), "Payload offset changed");

static JsonDocument parse(const char* json) {
    JsonDocument doc;
    assert(!deserializeJson(doc, json));
    return doc;
}

static JsonDocument query(CredStorage& storage, const char* type, uint32_t first = 0, uint32_t count = 15) {
    JsonDocument request, response;
    request["credType"] = type;
    request["credId"] = first;
    request["credCount"] = count;
    assert(buildCredResponse(storage, request.as<JsonObjectConst>(), response));
    assert(response["credType"] == type);
    JsonObjectConst rows = response["rows"];
    for (JsonPairConst column : rows) assert(column.value().size() == response["rowCount"].as<size_t>());
    return response;
}

static void reject(CredStorage& storage, const JsonDocument& request) {
    const auto before = Preferences::dumpNamespace("pins");
    JsonDocument response;
    assert(!applyCredChange(storage, request.as<JsonObjectConst>(), response));
    assert(response["success"] == false);
    assert(response["error"].is<const char*>());
    assert(before == Preferences::dumpNamespace("pins"));
}

int main() {
    Preferences::clearAll();
    CredStorage storage;
    assert(storage.begin());
    auto empty = query(storage, "ctCode");
    assert(empty["rowCount"] == 0);
    assert(empty["rows"]["codeid"].is<JsonArray>());
    assert(empty["rows"].size() == 5);
    JsonDocument result;

    auto password = parse(R"({"credType":"ctPin","pinid":"0","pinname":"Kule","pinvalue":"49786543","amount":"3","checkbox-unlimited":false,"datefrom":"2000-02-10","dateto":"2099-04-01","clicked":"savebutton","doornum":"4"})");
    assert(applyCredChange(storage, password.as<JsonObjectConst>(), result));
    uint32_t passwordId = result["credId"];
    assert(result["action"] == "created");
    password["pinid"] = passwordId;
    auto rows = query(storage, "ctPin");
    assert(rows["rowCount"] == 1);
    assert(rows["rows"].size() == 6);
    assert(rows["rows"]["pinid"][0].is<uint32_t>());
    assert(rows["rows"]["amount"][0] == "3");
    assert(rows["rows"]["datefrom"][0] == "2000-02-10");
    assert(rows["rows"]["dateto"][0] == "2099-04-01");
    CacheRecord stored;
    assert(storage.getCred(passwordId, stored, CredType::Password));
    assert(stored.validTo % 86400 == 86399);
    assert(stored.doorNum == 4);

    auto packet = parse(R"({"credType":"ctCode","codeid":"0","codename":"Tleskac","codevalue":"HZ1268956754M","codefrom":"0001-01-01","codeto":"9999-12-31","clicked":"savebutton"})");
    assert(applyCredChange(storage, packet.as<JsonObjectConst>(), result));
    uint32_t packetId = result["credId"];
    packet["codeid"] = packetId;
    assert(storage.getCred(packetId, stored, CredType::PacketNumber));
    assert(stored.remaining == 1 && stored.validFrom == 0 && stored.validTo == 0);
    rows = query(storage, "ctCode");
    assert(rows["rowCount"] == 1);
    assert(rows["rows"]["codevalue"][0] == "HZ1268956754M");
    assert(rows["rows"]["codefrom"][0] == "0001-01-01");
    assert(rows["rows"]["codeto"][0] == "9999-12-31");
    assert(rows["rows"]["pinid"].isNull());
    assert(query(storage, "ctPin", passwordId + 1)["rowCount"] == 0);
    assert(query(storage, "ctCode", packetId, 1)["rowCount"] == 1);
    assert(query(storage, "ctCode", 0, 0)["rowCount"] == 0);

    // Missing hidden fields preserve door and use count on update.
    stored.doorNum = 8;
    stored.remaining = 2;
    assert(storage.updateCred(stored));
    packet["codeto"] = "2099-12-30";
    assert(applyCredChange(storage, packet.as<JsonObjectConst>(), result));
    assert(storage.getCred(packetId, stored, CredType::PacketNumber));
    assert(stored.doorNum == 8 && stored.remaining == 2);
    password["amount"] = "5";
    password.remove("doornum");
    assert(applyCredChange(storage, password.as<JsonObjectConst>(), result));
    assert(storage.getCred(passwordId, stored, CredType::Password));
    assert(stored.doorNum == 4 && stored.remaining == 5);
    password["checkbox-unlimited"] = true;
    assert(applyCredChange(storage, password.as<JsonObjectConst>(), result));
    assert(query(storage, "ctPin")["rows"]["amount"][0] == "-1");

    // Restart retains the existing wire representation and discriminators.
    CredStorage restarted;
    assert(restarted.begin());
    assert(query(restarted, "ctPin")["rowCount"] == 1);
    assert(query(restarted, "ctCode")["rowCount"] == 1);
    assert(restarted.verifyCred("HZ1268956754M", CredType::PacketNumber) == 8);
    assert(restarted.usedCred(8));
    assert(restarted.getCred(packetId, stored, CredType::PacketNumber));
    assert(stored.remaining == 1);

    // UI examples with past dates are valid records but excluded from active lists.
    password["datefrom"] = "2026-02-10";
    password["dateto"] = "2026-04-01";
    assert(applyCredChange(restarted, password.as<JsonObjectConst>(), result));
    // Use an unambiguously past date, independent of the test machine clock.
    password["datefrom"] = "2000-01-01";
    password["dateto"] = "2000-01-02";
    assert(applyCredChange(restarted, password.as<JsonObjectConst>(), result));
    assert(query(restarted, "ctPin")["rowCount"] == 0);
    password["datefrom"] = "0001-01-01";
    password["dateto"] = "9999-12-31";
    assert(applyCredChange(restarted, password.as<JsonObjectConst>(), result));
    assert(query(restarted, "ctPin")["rowCount"] == 1);

    for (const char* date : {"2026-02-30", "2025-02-29", "2100-02-29", "2026-13-01", "tomorrow", "2026-01-01garbage"}) {
        auto bad = password;
        bad["datefrom"] = date;
        reject(restarted, bad);
    }
    auto leap = password;
    leap["datefrom"] = "2000-02-29";
    assert(applyCredChange(restarted, leap.as<JsonObjectConst>(), result));
    assert(query(restarted, "ctPin")["rows"]["datefrom"][0] == "2000-02-29");
    auto bad = password;
    bad["datefrom"] = "2099-02-02";
    bad["dateto"] = "2099-02-01";
    reject(restarted, bad);
    bad = password; bad.remove("clicked"); reject(restarted, bad);
    bad = password; bad["clicked"] = 1; reject(restarted, bad);
    bad = password; bad["credType"] = "all"; reject(restarted, bad);
    bad = password; bad["credType"] = 1; reject(restarted, bad);
    bad = password; bad["pinid"] = "-1"; reject(restarted, bad);
    bad = password; bad["pinid"] = "4294967296"; reject(restarted, bad);
    bad = password; bad["pinid"] = true; reject(restarted, bad);
    bad = password; bad["pinid"] = 1.5; reject(restarted, bad);
    bad = password; bad["pinid"] = packetId; reject(restarted, bad);
    bad = password; bad["pinname"] = "name-too-long"; reject(restarted, bad);
    bad = password; bad["pinvalue"] = "123456789"; reject(restarted, bad);
    bad = password; bad["pinvalue"] = 49786543; reject(restarted, bad);
    bad = password; bad["pinvalue"] = "abcdefgh"; reject(restarted, bad);
    bad = password; bad["pinvalue"] = "12345678"; reject(restarted, bad); // Identity is immutable.
    bad = password; bad["doornum"] = 255; reject(restarted, bad);
    bad = password; bad["checkbox-unlimited"] = "false"; reject(restarted, bad);
    bad = password; bad["checkbox-unlimited"] = false; bad["amount"] = "2x"; reject(restarted, bad);
    bad["amount"] = "2147483648"; reject(restarted, bad);
    bad = packet; bad["codevalue"] = std::string(PACKET_NUMBER_MAX + 1, 'A'); reject(restarted, bad);
    bad = packet; bad["codeid"] = passwordId; reject(restarted, bad);
    bad = packet; bad["clicked"] = "deletebutton"; bad["codeid"] = 0; reject(restarted, bad);

    auto request = parse(R"({"credId":0,"credCount":15,"credType":"ctPin"})");
    request["credCount"] = -1;
    assert(!buildCredResponse(restarted, request.as<JsonObjectConst>(), result));
    request["credCount"] = 101;
    assert(!buildCredResponse(restarted, request.as<JsonObjectConst>(), result));
    request["credCount"] = 1;
    request["credType"] = "unknown";
    assert(!buildCredResponse(restarted, request.as<JsonObjectConst>(), result));

    // Explicit deletion checks full identity and type, including packet text.
    packet["clicked"] = "deletebutton";
    assert(applyCredChange(restarted, packet.as<JsonObjectConst>(), result));
    assert(query(restarted, "ctCode")["rowCount"] == 0);
    reject(restarted, packet); // Already deleted.
    password["clicked"] = "deletebutton";
    assert(applyCredChange(restarted, password.as<JsonObjectConst>(), result));
    assert(query(restarted, "ctPin")["rowCount"] == 0);
    std::cout << "PASS: credential protocol CRUD, dates, paging, validation and NVS layout\n";
}
