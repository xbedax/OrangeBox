#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
constexpr int ESP_OK = 0;
constexpr int ESP_PARTITION_TYPE_DATA = 1;
constexpr int ESP_PARTITION_SUBTYPE_DATA_SPIFFS = 130;
struct esp_partition_t { size_t size; };
inline std::vector<uint8_t> testPartitionBytes;
inline const esp_partition_t* esp_partition_find_first(int, int, const char*) {
    static esp_partition_t partition;
    partition.size = testPartitionBytes.size();
    return partition.size ? &partition : nullptr;
}
inline int esp_partition_read(const esp_partition_t*, size_t offset, void* target, size_t size) {
    memcpy(target, testPartitionBytes.data() + offset, size);
    return ESP_OK;
}
