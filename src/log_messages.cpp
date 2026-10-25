#include "logger.h"
#include <cctype>

namespace {
const LogMessageDefinition messages[] = {
#define LOG_MESSAGE(id, text, shortText) {id, text, shortText},
#include "log_messages.def"
#undef LOG_MESSAGE
};
}

const LogMessageDefinition* findLogMessage(uint16_t id) {
    // The dictionary lives in flash on ESP32; no runtime map or allocation.
    size_t low = 0, high = sizeof(messages) / sizeof(messages[0]);
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (messages[mid].id < id) low = mid + 1;
        else high = mid;
    }
    return low < sizeof(messages) / sizeof(messages[0]) && messages[low].id == id ? &messages[low] : nullptr;
}

uint8_t logMessageParamCount(const LogMessageDefinition& definition) {
    uint8_t maximum = 0;
    for (const char* text : {definition.text, definition.shortText}) {
        if (!text) continue;
        while (*text) {
            if (text[0] == '{' && text[1] == 'p') {
                text += 2;
                unsigned index = 0;
                while (*text >= '0' && *text <= '9' && index <= LOG_MAX_PARAMS)
                    index = index * 10 + (*text++ - '0');
                if (*text != '}' || index == 0 || index > LOG_MAX_PARAMS) return 255;
                if (index > maximum) maximum = index;
            } else ++text;
        }
    }
    return maximum;
}
