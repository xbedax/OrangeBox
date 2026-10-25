#pragma once

#include <cstddef>
#include <cstdint>

namespace QrPayload {

inline int hexValue(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

// Empty text is a valid, disabled command. Hex requires an explicit prefix.
inline bool parse(const char* input, uint8_t* output, size_t capacity, size_t& length) {
    length = 0;
    if (input == nullptr || output == nullptr) return false;
    while (*input == ' ' || *input == '\t') ++input;
    if (input[0] == 'h' && input[1] == 'e' && input[2] == 'x'
        && (input[3] == ':' || input[3] == ' ' || input[3] == '\t')) {
        input += 3;
        while (*input == ' ' || *input == '\t') ++input;
        if (*input == ':') ++input;
        int high = -1;
        for (; *input; ++input) {
            if (*input == ' ' || *input == '\t' || *input == ',' || *input == ':' || *input == '-') continue;
            const int digit = hexValue(*input);
            if (digit < 0) return false;
            if (high < 0) high = digit;
            else {
                if (length == capacity) return false;
                output[length++] = static_cast<uint8_t>((high << 4) | digit);
                high = -1;
            }
        }
        return high < 0 && length > 0;
    }

    for (; *input; ++input) {
        uint8_t value = static_cast<uint8_t>(*input);
        if (*input == '\\' && input[1] != '\0') {
            ++input;
            switch (*input) {
                case 'r': value = '\r'; break;
                case 'n': value = '\n'; break;
                case 't': value = '\t'; break;
                case '0': value = 0; break;
                case '\\': value = '\\'; break;
                case 'x': {
                    if (input[1] == '\0' || input[2] == '\0') return false;
                    const int high = hexValue(input[1]);
                    const int low = hexValue(input[2]);
                    if (high < 0 || low < 0) return false;
                    value = static_cast<uint8_t>((high << 4) | low);
                    input += 2;
                    break;
                }
                default: value = static_cast<uint8_t>(*input); break;
            }
        }
        if (length == capacity) return false;
        output[length++] = value;
    }
    return true;
}

} // namespace QrPayload
