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
#include "cred_protocol.h"
#include "logger.h"

Logger logger;
void Logger::logPrint(uint8_t, const String&, const char*, uint8_t) {}

CredStorage credStorage;

static std::string formatCredKey(uint32_t id)
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
        << "  id=" << rec.credId
        << " name=\"" << rec.name << "\""
        << " cred=\"" << rec.credential() << "\""
        << " door=" << static_cast<unsigned>(rec.doorNum)
        << " from=" << rec.validFrom
        << " to=" << rec.validTo
        << " remaining=" << rec.remaining
        << "\n";
}

static void dumpCache()
{
    const auto& cache = credStorage.debugCache();
    std::cout << "\n[CACHE] active records: " << cache.size() << "\n";
    if (cache.empty()) {
        std::cout << "  <empty>\n";
    }

    for (const auto& rec : cache) {
        printCacheRecord(rec);
    }

    std::vector<CacheRecord> creds;
    size_t total = credStorage.getCreds(0, 255, creds, CredType::All);
    std::cout << "[CACHE:getCreds] total=" << total << "\n";
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
        if (bytes.size() == sizeof(CredRecord)) {
            CredRecord rec = {};
            std::memcpy(&rec, bytes.data(), sizeof(rec));
            std::cout
                << " id=" << rec.credId
                << " prev=" << rec.prevId
                << " next=" << rec.nextId
                << " door=" << static_cast<unsigned>(rec.doorNum)
                << " name=\"" << rec.name << "\""
                << " cred=\"" << rec.credential() << "\""
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

static bool seedRecordFromJson(JsonObjectConst data, CredRecord& rec, std::string& key)
{
    rec = {};
    if (!readSeedId(data, "credId", "id", rec.credId) && !readUint32(data, "pinId", rec.credId)) {
        if (!parseUint32Value(data["key"], rec.credId)) {
            std::cout << "[SEED:ERROR] record missing credId/id/key\n";
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
        key = formatCredKey(rec.credId);
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

        CredRecord rec = {};
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

static void applyMessage(JsonObjectConst root)
{
    const char* command = root["_command_"] | "";
    JsonObjectConst data = root["data"].as<JsonObjectConst>();
    JsonDocument response;
    if (std::strcmp(command, COMM_GET_CREDS) == 0) buildCredResponse(credStorage, data, response);
    else if (std::strcmp(command, COMM_SET_CRED) == 0) applyCredChange(credStorage, data, response);
    else {
        std::cout << "[ERROR] unknown command\n";
        return;
    }
    std::string json;
    serializeJson(response, json);
    std::cout << "[c_setcred] " << json << "\n";
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
        std::cout << "\n[BOOT] Raw Preferences before CredStorage::begin()\n";
        dumpPreferences();
    }

    credStorage.begin();
    std::cout << "\n[BOOT] After CredStorage::begin()\n";
    dumpAll();

    std::cout
        << "Pass store console\n"
        << "Usage: run_pass_store_console.cmd [optional-seed-json]\n"
        << "Input: one JSON object per line, q to quit.\n"
        << "Seed format: array, pins array, records array, or preferences.pins array.\n"
        << "Commands: get_creds and set_cred with a data object; see test/cred_protocol.md.\n";

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
