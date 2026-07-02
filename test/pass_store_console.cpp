#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <ArduinoJson.h>

#include "Arduino.h"
#include "Preferences.h"
#include "config.h"
#include "pass_store.h"

PinStorage pinStorage;

static std::string formatPinKey(uint32_t id)
{
    char key[ID_LENGTH + 1];
    std::snprintf(key, sizeof(key), "%08X", id);
    return key;
}

static bool parseUint32Value(JsonVariantConst value, uint32_t& out)
{
    if (value.isNull()) {
        return false;
    }

    if (value.is<const char*>()) {
        const char* text = value.as<const char*>();
        if (std::strcmp(text, "FIRST") == 0 || std::strcmp(text, "FIRST_KEY") == 0) {
            out = FIRST_KEY;
            return true;
        }
        if (std::strcmp(text, "LAST") == 0 || std::strcmp(text, "LAST_KEY") == 0) {
            out = LAST_KEY;
            return true;
        }

        int base = 10;
        if (std::strncmp(text, "0x", 2) == 0 || std::strncmp(text, "0X", 2) == 0) {
            base = 16;
        } else if (std::strlen(text) == ID_LENGTH
            && std::all_of(text, text + ID_LENGTH, [](unsigned char ch) { return std::isxdigit(ch); })) {
            base = 16;
        }

        char* endptr = nullptr;
        unsigned long parsed = std::strtoul(text, &endptr, base);
        if (endptr == text || *endptr != '\0') {
            return false;
        }
        out = static_cast<uint32_t>(parsed);
        return true;
    }

    out = value.as<uint32_t>();
    return true;
}

static bool readUint32(JsonObjectConst data, const char* key, uint32_t& out)
{
    return parseUint32Value(data[key], out);
}

static uint64_t readUint64Or(JsonObjectConst data, const char* key, uint64_t fallback)
{
    JsonVariantConst value = data[key];
    if (value.isNull()) {
        return fallback;
    }

    if (value.is<const char*>()) {
        char* endptr = nullptr;
        unsigned long long parsed = std::strtoull(value.as<const char*>(), &endptr, 10);
        if (endptr == value.as<const char*>() || *endptr != '\0') {
            return fallback;
        }
        return static_cast<uint64_t>(parsed);
    }

    return value.as<uint64_t>();
}

static int32_t readInt32Or(JsonObjectConst data, const char* key, int32_t fallback)
{
    JsonVariantConst value = data[key];
    if (value.isNull()) {
        return fallback;
    }

    if (value.is<const char*>()) {
        char* endptr = nullptr;
        long parsed = std::strtol(value.as<const char*>(), &endptr, 10);
        if (endptr == value.as<const char*>() || *endptr != '\0') {
            return fallback;
        }
        return static_cast<int32_t>(parsed);
    }

    return value.as<int32_t>();
}

static bool copyStringField(JsonObjectConst data, const char* key, char* dest, size_t destSize)
{
    if (destSize == 0) {
        return false;
    }

    JsonVariantConst value = data[key];
    if (value.isNull()) {
        dest[0] = '\0';
        return false;
    }

    if (value.is<const char*>()) {
        std::strncpy(dest, value.as<const char*>(), destSize - 1);
        dest[destSize - 1] = '\0';
        return true;
    }

    std::string converted;
    if (value.is<int64_t>()) {
        converted = std::to_string(value.as<int64_t>());
    } else {
        converted = std::to_string(value.as<uint64_t>());
    }
    std::strncpy(dest, converted.c_str(), destSize - 1);
    dest[destSize - 1] = '\0';
    return true;
}

static void printCacheRecord(const CacheRecord& rec)
{
    std::cout
        << "  id=" << rec.pinId
        << " name=\"" << rec.name << "\""
        << " pin=\"" << rec.pin << "\""
        << " door=" << static_cast<unsigned>(rec.doorNum)
        << " from=" << rec.validFrom
        << " to=" << rec.validTo
        << " remaining=" << rec.remaining
        << "\n";
}

static void dumpCache()
{
    const auto& cache = pinStorage.debugCache();
    std::cout << "\n[CACHE] active records: " << cache.size() << "\n";
    if (cache.empty()) {
        std::cout << "  <empty>\n";
    }

    for (const auto& rec : cache) {
        printCacheRecord(rec);
    }

    String pinsTable;
    size_t total = pinStorage.getPins(0, 255, pinsTable);
    std::cout << "[CACHE:getPins] total=" << total << " html=" << pinsTable << "\n";
}

static void printHexBytes(const std::vector<uint8_t>& bytes)
{
    for (uint8_t value : bytes) {
        std::cout
            << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(value);
    }
    std::cout << std::dec << std::setfill(' ');
}

static void dumpPreferences()
{
    const auto& ns = Preferences::dumpNamespace("pins");
    std::vector<std::string> keys;
    keys.reserve(ns.size());
    for (const auto& item : ns) {
        keys.push_back(item.first);
    }
    std::sort(keys.begin(), keys.end());

    std::cout << "\n[PREFERENCES:pins] records: " << keys.size() << "\n";
    if (keys.empty()) {
        std::cout << "  <empty>\n";
    }

    for (const auto& key : keys) {
        const auto& bytes = ns.at(key);
        std::cout << "  key=" << key << " bytes=" << bytes.size();
        if (bytes.size() == sizeof(PinRecord)) {
            PinRecord rec = {};
            std::memcpy(&rec, bytes.data(), sizeof(rec));
            std::cout
                << " id=" << rec.pinId
                << " prev=" << rec.prevId
                << " next=" << rec.nextId
                << " door=" << static_cast<unsigned>(rec.doorNum)
                << " name=\"" << rec.name << "\""
                << " pin=\"" << rec.pin << "\""
                << " from=" << rec.validFrom
                << " to=" << rec.validTo
                << " remaining=" << rec.remaining;
        } else {
            std::cout << " raw=0x";
            printHexBytes(bytes);
        }
        std::cout << "\n";
    }
}

static void dumpAll()
{
    dumpCache();
    dumpPreferences();
    std::cout << "\n";
}

static JsonArrayConst pickSeedRecords(JsonDocument& doc)
{
    if (doc.is<JsonArrayConst>()) {
        return doc.as<JsonArrayConst>();
    }

    if (!doc.is<JsonObjectConst>()) {
        return JsonArrayConst();
    }

    JsonObjectConst root = doc.as<JsonObjectConst>();
    if (root["pins"].is<JsonArrayConst>()) {
        return root["pins"].as<JsonArrayConst>();
    }
    if (root["records"].is<JsonArrayConst>()) {
        return root["records"].as<JsonArrayConst>();
    }
    if (root["preferences"]["pins"].is<JsonArrayConst>()) {
        return root["preferences"]["pins"].as<JsonArrayConst>();
    }
    return JsonArrayConst();
}

static uint64_t readSeedUint64Or(JsonObjectConst data, const char* primary, const char* alias, uint64_t fallback)
{
    if (!data[primary].isNull()) {
        return readUint64Or(data, primary, fallback);
    }
    return readUint64Or(data, alias, fallback);
}

static int32_t readSeedInt32Or(JsonObjectConst data, const char* primary, const char* alias, int32_t fallback)
{
    if (!data[primary].isNull()) {
        return readInt32Or(data, primary, fallback);
    }
    return readInt32Or(data, alias, fallback);
}

static bool readSeedId(JsonObjectConst data, const char* primary, const char* alias, uint32_t& out)
{
    if (readUint32(data, primary, out)) {
        return true;
    }
    return readUint32(data, alias, out);
}

static bool seedRecordFromJson(JsonObjectConst data, PinRecord& rec, std::string& key)
{
    rec = {};
    if (!readSeedId(data, "pinId", "id", rec.pinId)) {
        if (!parseUint32Value(data["key"], rec.pinId)) {
            std::cout << "[SEED:ERROR] record missing pinId/id/key\n";
            return false;
        }
    }

    readSeedId(data, "prevId", "prev", rec.prevId);
    readSeedId(data, "nextId", "next", rec.nextId);

    uint32_t doorNum = 0;
    if (readSeedId(data, "doorNum", "door", doorNum)) {
        rec.doorNum = static_cast<uint8_t>(doorNum);
    }

    copyStringField(data, "name", rec.name, sizeof(rec.name));
    copyStringField(data, "pin", rec.pin, sizeof(rec.pin));
    rec.validFrom = readSeedUint64Or(data, "validFrom", "from", 0);
    rec.validTo = readSeedUint64Or(data, "validTo", "to", 0);
    rec.remaining = readSeedInt32Or(data, "remaining", "amount", 0);

    if (data["key"].is<const char*>()) {
        key = data["key"].as<const char*>();
    } else {
        key = formatPinKey(rec.pinId);
    }
    return true;
}

static bool loadPreferencesSeed(const char* path)
{
    if (path == nullptr || path[0] == '\0') {
        return true;
    }

    std::ifstream input(path);
    if (!input) {
        std::cout << "[SEED:ERROR] cannot open seed file: " << path << "\n";
        return false;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, buffer.str());
    if (error) {
        std::cout << "[SEED:ERROR] JSON parse failed: " << error.c_str() << "\n";
        return false;
    }

    JsonArrayConst records = pickSeedRecords(doc);
    if (records.isNull()) {
        std::cout << "[SEED:ERROR] seed file must contain an array, pins array, records array, or preferences.pins array\n";
        return false;
    }

    Preferences prefs;
    prefs.begin("pins", false);

    size_t loaded = 0;
    for (JsonVariantConst item : records) {
        if (!item.is<JsonObjectConst>()) {
            std::cout << "[SEED:ERROR] skipping non-object seed item\n";
            continue;
        }

        PinRecord rec = {};
        std::string key;
        if (!seedRecordFromJson(item.as<JsonObjectConst>(), rec, key)) {
            continue;
        }

        prefs.putBytes(key.c_str(), &rec, sizeof(rec));
        loaded++;
    }

    std::cout << "[SEED] loaded " << loaded << " Preferences record(s) from " << path << "\n";
    return true;
}

static bool buildSaveRecord(JsonObjectConst data, CacheRecord& rec)
{
    rec = {};
    readUint32(data, "pinid", rec.pinId);

    if (!copyStringField(data, "pinname", rec.name, sizeof(rec.name))) {
        std::cout << "[ERROR] save: missing pinname\n";
        return false;
    }

    if (!copyStringField(data, "pin", rec.pin, sizeof(rec.pin))) {
        std::cout << "[ERROR] save: missing pin\n";
        return false;
    }

    rec.validFrom = readUint64Or(data, "datefrom", DATE_FROM_UNLIMITED);
    rec.validTo = readUint64Or(data, "dateto", DATE_TO_UNLIMITED);
    rec.remaining = readInt32Or(data, "amount", -1);

    uint32_t doorNum = 1;
    if (!readUint32(data, "doornum", doorNum)) {
        doorNum = 1;
    }
    if (doorNum == 0 || doorNum > 254) {
        std::cout << "[ERROR] save: invalid doornum=" << doorNum << "\n";
        return false;
    }
    rec.doorNum = static_cast<uint8_t>(doorNum);
    return true;
}

static void handleSave(JsonObjectConst data)
{
    CacheRecord rec = {};
    if (!buildSaveRecord(data, rec)) {
        return;
    }

    if (rec.pinId != 0) {
        bool updated = pinStorage.updatePin(rec);
        std::cout << (updated ? "[OK] updated pin " : "[ERROR] update failed for pin ")
                  << rec.pinId << "\n";
        return;
    }

    uint32_t newId = pinStorage.addPin(rec);
    if (newId != 0) {
        std::cout << "[OK] added pin " << newId << "\n";
    } else {
        std::cout << "[ERROR] add failed\n";
    }
}

static void handleDelete(JsonObjectConst data)
{
    CacheRecord rec = {};
    if (!readUint32(data, "pinid", rec.pinId) || rec.pinId == 0) {
        std::cout << "[ERROR] delete: missing pinid\n";
        return;
    }
    if (!copyStringField(data, "pinname", rec.name, sizeof(rec.name))) {
        std::cout << "[ERROR] delete: missing pinname\n";
        return;
    }
    if (!copyStringField(data, "pin", rec.pin, sizeof(rec.pin))) {
        std::cout << "[ERROR] delete: missing pin; PinStorage::removePin verifies name and pin\n";
        return;
    }

    uint32_t deleted = pinStorage.removePin(rec);
    if (deleted == rec.pinId) {
        std::cout << "[OK] deleted pin " << deleted << "\n";
    } else {
        std::cout << "[ERROR] delete failed for pin " << rec.pinId << "\n";
    }
}

static void handleUse(JsonObjectConst data)
{
    char pin[MAX_PIN_LENGTH + 1] = {};
    if (!copyStringField(data, "pin", pin, sizeof(pin))) {
        std::cout << "[ERROR] use: missing pin\n";
        return;
    }
    uint8_t doorNum = pinStorage.usePin(pin);
    std::cout << "[USE] pin=\"" << pin << "\" door=" << static_cast<unsigned>(doorNum) << "\n";
}

static bool isCommand(const char* command, const char* symbolic, const char* wire)
{
    if (command == nullptr || command[0] == '\0') {
        return false;
    }
    return std::strcmp(command, symbolic) == 0 || std::strcmp(command, wire) == 0;
}

static void applyMessage(JsonObjectConst root)
{
    const char* command = root["_command_"] | "";
    JsonObjectConst data = root["data"].is<JsonObjectConst>()
        ? root["data"].as<JsonObjectConst>()
        : root;

    const char* clicked = data["clicked"] | PIN_SAVE_BEACON;
    if (isCommand(command, "COMM_GET_PINS", COMM_GET_PINS)) {
        std::cout << "[OK] get pins\n";
        return;
    }

    if (isCommand(command, "USE_PIN", "use_pin")) {
        handleUse(data);
        return;
    }

    if (!isCommand(command, "COMM_SET_PIN", COMM_SET_PIN) && command[0] != '\0') {
        std::cout << "[ERROR] unknown command: " << command << "\n";
        return;
    }

    if (std::strcmp(clicked, PIN_DELETE_BEACON) == 0) {
        handleDelete(data);
    } else if (std::strcmp(clicked, PIN_SAVE_BEACON) == 0) {
        handleSave(data);
    } else {
        std::cout << "[ERROR] unknown clicked action: " << clicked << "\n";
    }
}

int main(int argc, char** argv)
{
    Serial.begin(9600);
    Preferences::clearAll();
    const char* seedPath = argc > 1 ? argv[1] : nullptr;
    if (!loadPreferencesSeed(seedPath)) {
        return 1;
    }
    if (seedPath != nullptr) {
        std::cout << "\n[BOOT] Raw Preferences before PinStorage::begin()\n";
        dumpPreferences();
    }

    pinStorage.begin();
    std::cout << "\n[BOOT] After PinStorage::begin()\n";
    dumpAll();

    std::cout
        << "Pass store console\n"
        << "Usage: run_pass_store_console.cmd [optional-seed-json]\n"
        << "Input: one JSON object per line, q to quit.\n"
        << "Seed format: array, pins array, records array, or preferences.pins array.\n"
        << "Examples:\n"
        << "{\"clicked\":\"savebutton\",\"pinname\":\"demo\",\"pin\":\"123456\",\"doornum\":\"1\",\"amount\":-1}\n"
        << "{\"clicked\":\"deletebutton\",\"pinid\":1,\"pinname\":\"demo\",\"pin\":\"123456\"}\n\n";

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "q" || line == "Q") {
            break;
        }
        if (line.empty()) {
            continue;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, line);
        if (error) {
            std::cout << "[ERROR] JSON parse failed: " << error.c_str() << "\n";
            dumpAll();
            continue;
        }
        if (!doc.is<JsonObjectConst>()) {
            std::cout << "[ERROR] JSON root must be an object\n";
            dumpAll();
            continue;
        }

        applyMessage(doc.as<JsonObjectConst>());
        dumpAll();
    }

    return 0;
}
