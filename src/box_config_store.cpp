#include "box_config.h"
#include <LittleFS.h>
#include <esp_partition.h>
#include <algorithm>

namespace {
bool partitionIsBlank() {
    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
    if (!partition) return false;
    uint8_t bytes[256];
    for (size_t offset = 0; offset < partition->size; offset += sizeof(bytes)) {
        size_t length = std::min(sizeof(bytes), static_cast<size_t>(partition->size - offset));
        if (esp_partition_read(partition, offset, bytes, length) != ESP_OK) return false;
        for (size_t i = 0; i < length; ++i) if (bytes[i] != 0xff) return false;
        if (offset % 65536 == 0) delay(1);
    }
    return true;
}
}

bool BoxConfigStore::begin() {
    // A factory-erased partition can be initialized. Never erase existing data,
    // including an unreadable/corrupt filesystem or another filesystem format.
    mounted = LittleFS.begin(false);
    if (!mounted && partitionIsBlank()) mounted = LittleFS.begin(true);
    return mounted;
}

bool BoxConfigStore::load(BoxConfig& config, String& error) {
    if (!mounted) { error = "LittleFS unavailable"; return false; }
    if (!LittleFS.exists(Path)) return true;
    File file = LittleFS.open(Path, "r");
    if (!file || file.size() == 0 || file.size() > MaxBytes) { error = "Invalid configuration size"; return false; }
    JsonDocument doc;
    auto result = deserializeJson(doc, file, DeserializationOption::NestingLimit(8));
    if (result) { error = "Invalid configuration JSON"; return false; }
    return decode(doc.as<JsonVariantConst>(), config, error);
}

bool BoxConfigStore::commitDownloaded(String& error) {
    if (!mounted) { error = "LittleFS unavailable"; return false; }
    File file = LittleFS.open(PendingPath, "r");
    if (!file || !file.size() || file.size() > MaxBytes) { error = "Invalid download size"; return false; }
    JsonDocument doc;
    auto result = deserializeJson(doc, file, DeserializationOption::NestingLimit(8));
    file.close();
    BoxConfig candidate;
    if (result) { error = "Invalid downloaded JSON"; return false; }
    if (!decode(doc.as<JsonVariantConst>(), candidate, error, true)) return false;
    // LittleFS rename replaces the destination atomically, including after power loss.
    if (!LittleFS.rename(PendingPath, Path)) { error = "Configuration rename failed"; return false; }
    return true;
}
