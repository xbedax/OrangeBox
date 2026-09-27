#pragma once
#include "Arduino.h"
#include <deque>
class Stream {
public:
    std::deque<uint8_t> input;
    int available() { return static_cast<int>(input.size()); }
    int read() { if (input.empty()) return -1; auto value = input.front(); input.pop_front(); return value; }
    size_t write(const uint8_t*, size_t length) { return length; }
};
