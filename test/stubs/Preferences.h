#ifndef TEST_STUBS_PREFERENCES_H
#define TEST_STUBS_PREFERENCES_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

class Preferences {
public:
    bool begin(const char* name, bool readOnly = false)
    {
        namespaceName = name ? name : "";
        readonly = readOnly;
        return true;
    }

    size_t putBytes(const char* key, const void* value, size_t len)
    {
        if (readonly || key == nullptr || (value == nullptr && len > 0)) {
            return 0;
        }

        auto& slot = data[std::string(key)];
        slot.resize(len);
        if (len > 0) {
            std::memcpy(slot.data(), value, len);
        }
        return len;
    }

    size_t getBytes(const char* key, void* value, size_t maxLen) const
    {
        if (key == nullptr || value == nullptr) {
            return 0;
        }

        auto found = data.find(std::string(key));
        if (found == data.end()) {
            return 0;
        }

        if (found->second.size() > maxLen) {
            return 0;
        }

        if (!found->second.empty()) {
            std::memcpy(value, found->second.data(), found->second.size());
        }
        return found->second.size();
    }

    bool remove(const char* key)
    {
        if (readonly || key == nullptr) {
            return false;
        }
        return data.erase(std::string(key)) > 0;
    }

    bool isKey(const char* key) const
    {
        if (key == nullptr) {
            return false;
        }
        return data.find(std::string(key)) != data.end();
    }

private:
    std::string namespaceName;
    bool readonly = false;
    std::unordered_map<std::string, std::vector<uint8_t>> data;
};

#endif // TEST_STUBS_PREFERENCES_H
