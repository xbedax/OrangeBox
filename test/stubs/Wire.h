#ifndef TEST_STUBS_WIRE_H
#define TEST_STUBS_WIRE_H

#include <cstddef>
#include <cstdint>
#include <queue>

class WireStub {
public:
    void begin(int = 0, int = 0) {}
    void setClock(unsigned long) {}

    void requestFrom(uint8_t, size_t)
    {
        readQueue = std::queue<uint8_t>();
        if (pendingKeys.empty()) {
            return;
        }

        size_t len = pendingKeys.size();
        if (len > 255) {
            len = 255;
        }
        readQueue.push(static_cast<uint8_t>(len));
        for (size_t i = 0; i < len; ++i) {
            readQueue.push(pendingKeys.front());
            pendingKeys.pop();
        }
    }

    int available() const
    {
        return static_cast<int>(readQueue.size());
    }

    uint8_t read()
    {
        if (readQueue.empty()) {
            return 0;
        }
        uint8_t value = readQueue.front();
        readQueue.pop();
        return value;
    }

    void pushKey(uint8_t key)
    {
        pendingKeys.push(key);
    }

private:
    std::queue<uint8_t> pendingKeys;
    std::queue<uint8_t> readQueue;
};

inline WireStub Wire;

#endif // TEST_STUBS_WIRE_H
