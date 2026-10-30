#pragma once
#include <algorithm>
#include <cstring>
#include <map>
#include <string>

class File {
public:
    explicit File(std::string* contents = nullptr) : contents(contents) {}
    explicit operator bool() const { return contents != nullptr; }
    size_t size() const { return contents ? contents->size() : 0; }
    int read() { return contents && offset < size() ? static_cast<unsigned char>((*contents)[offset++]) : -1; }
    size_t readBytes(char* target, size_t count) {
        size_t length = std::min(count, size() - offset);
        if (length) memcpy(target, contents->data() + offset, length);
        offset += length;
        return length;
    }
    void close() {}
private:
    std::string* contents;
    size_t offset = 0;
};

struct TestLittleFS {
    std::map<std::string, std::string> files;
    bool mountOk = true, failRename = false;
    int formatCount = 0;
    bool begin(bool format) { if (format) { ++formatCount; mountOk = true; } return mountOk; }
    bool exists(const char* path) { return files.count(path) != 0; }
    File open(const char* path, const char*) { return exists(path) ? File(&files[path]) : File(); }
    bool rename(const char* from, const char* to) {
        if (failRename || !exists(from)) return false;
        files[to] = files[from]; files.erase(from); return true;
    }
};
inline TestLittleFS LittleFS;
