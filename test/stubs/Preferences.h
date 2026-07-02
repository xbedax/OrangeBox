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
    using Blob = std::vector<uint8_t>;
    using NamespaceData = std::unordered_map<std::string, Blob>;

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

        auto& slot = currentNamespace()[std::string(key)];
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

        const auto& ns = currentNamespace();
        auto found = ns.find(std::string(key));
        if (found == ns.end()) {
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
        return currentNamespace().erase(std::string(key)) > 0;
    }

    bool clear()
    {
        if (readonly) {
            return false;
        }
        currentNamespace().clear();
        return true;
    }

    bool isKey(const char* key) const
    {
        if (key == nullptr) {
            return false;
        }
        const auto& ns = currentNamespace();
        return ns.find(std::string(key)) != ns.end();
    }

    static const NamespaceData& dumpNamespace(const char* name)
    {
        static const NamespaceData empty;
        auto found = allNamespaces().find(std::string(name ? name : ""));
        if (found == allNamespaces().end()) {
            return empty;
        }
        return found->second;
    }

    static void clearAll()
    {
        allNamespaces().clear();
    }

private:
    std::string namespaceName;
    bool readonly = false;

    NamespaceData& currentNamespace() const
    {
        return allNamespaces()[namespaceName];
    }

    static std::unordered_map<std::string, NamespaceData>& allNamespaces()
    {
        static std::unordered_map<std::string, NamespaceData> namespaces;
        return namespaces;
    }
};

#endif // TEST_STUBS_PREFERENCES_H
