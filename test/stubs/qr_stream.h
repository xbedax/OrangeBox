#pragma once
#include "Arduino.h"
#include <deque>
#include <vector>
class Stream {
public:
    std::deque<uint8_t> input;
    std::vector<uint8_t> output;
    int available() { return static_cast<int>(input.size()); }
    int read() { if (input.empty()) return -1; auto value = input.front(); input.pop_front(); return value; }
    size_t write(const uint8_t* data, size_t length) {
        output.insert(output.end(), data, data + length);
        return length;
    }
};
