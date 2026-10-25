#include "logger.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <inttypes.h>

static_assert(LOG_MAX_PARAMS > 0 && LOG_MAX_PARAMS <= 255, "Invalid log parameter limit");
static_assert(LOG_PARAM_DATA_MAX_LEN > 0 && LOG_PARAM_DATA_MAX_LEN <= 65535, "Invalid log block size");

bool LogParamBlock::append(const LogParam& param) {
    const void* source = nullptr;
    size_t size = 0;
    switch (param.type) {
        case LogParam::Type::Signed: source = &param.value.signedValue; size = sizeof(int64_t); break;
        case LogParam::Type::Unsigned: source = &param.value.unsignedValue; size = sizeof(uint64_t); break;
        case LogParam::Type::Real:
            if (!std::isfinite(param.value.realValue)) return false;
            source = &param.value.realValue; size = sizeof(double); break;
        case LogParam::Type::Boolean: source = &param.value.boolValue; size = sizeof(bool); break;
        case LogParam::Type::Text:
            if (!param.value.textValue) return false;
            source = param.value.textValue;
            size = strnlen(param.value.textValue, LOG_PARAM_DATA_MAX_LEN) + 1;
            break;
        default: return false;
    }
    if (count >= LOG_MAX_PARAMS || used > sizeof(data) || size + 3 > sizeof(data) - used) return false;
    data[used++] = static_cast<uint8_t>(param.type);
    data[used++] = static_cast<uint8_t>(size);
    data[used++] = static_cast<uint8_t>(size >> 8);
    memcpy(data + used, source, size);
    used += size;
    ++count;
    return true;
}

bool LogParamBlock::get(uint8_t index, LogParam& param) const {
    if (index >= count || count > LOG_MAX_PARAMS || used > sizeof(data)) return false;
    size_t cursor = 0;
    for (uint8_t i = 0; i <= index; ++i) {
        if (cursor + 3 > used) return false;
        auto type = static_cast<LogParam::Type>(data[cursor]);
        size_t size = data[cursor + 1] | (size_t(data[cursor + 2]) << 8);
        cursor += 3;
        if (size > used - cursor) return false;
        if (i == index) {
            param.type = type;
            switch (type) {
                case LogParam::Type::Signed:
                    if (size != sizeof(int64_t)) return false;
                    memcpy(&param.value.signedValue, data + cursor, size); break;
                case LogParam::Type::Unsigned:
                    if (size != sizeof(uint64_t)) return false;
                    memcpy(&param.value.unsignedValue, data + cursor, size); break;
                case LogParam::Type::Real:
                    if (size != sizeof(double)) return false;
                    memcpy(&param.value.realValue, data + cursor, size);
                    if (!std::isfinite(param.value.realValue)) return false;
                    break;
                case LogParam::Type::Boolean:
                    if (size != sizeof(bool) || data[cursor] > 1) return false;
                    param.value.boolValue = data[cursor] != 0; break;
                case LogParam::Type::Text:
                    if (!size || data[cursor + size - 1] != 0) return false;
                    param.value.textValue = reinterpret_cast<const char*>(data + cursor); break;
                default: return false;
            }
            return true;
        }
        cursor += size;
    }
    return false;
}

namespace {
struct Writer {
    char* out;
    size_t capacity;
    size_t used = 0;
    bool ok = true;
    void put(char c) {
        if (used + 1 < capacity) out[used++] = c;
        else ok = false;
    }
    void text(const char* s) { while (*s) put(*s++); }
    void param(const LogParam& p) {
        char buffer[64];
        switch (p.type) {
            case LogParam::Type::Signed: snprintf(buffer, sizeof(buffer), "%" PRId64, p.value.signedValue); text(buffer); break;
            case LogParam::Type::Unsigned: snprintf(buffer, sizeof(buffer), "%" PRIu64, p.value.unsignedValue); text(buffer); break;
            case LogParam::Type::Real: snprintf(buffer, sizeof(buffer), "%.17g", p.value.realValue); text(buffer); break;
            case LogParam::Type::Boolean: text(p.value.boolValue ? "true" : "false"); break;
            case LogParam::Type::Text: text(p.value.textValue); break;
        }
    }
    bool finish() { if (capacity) out[used] = '\0'; return ok; }
};
}

bool formatLogMessage(uint16_t id, const LogParamBlock& params, bool shortText, char* output, size_t capacity) {
    if (!output || !capacity) return false;
    Writer writer{output, capacity};
    const auto* definition = findLogMessage(id);
    if (!definition) {
        char prefix[32];
        snprintf(prefix, sizeof(prefix), "Unknown message A%03u", unsigned(id));
        writer.text(prefix);
        for (uint8_t i = 0; i < params.count; ++i) {
            LogParam param;
            if (!params.get(i, param)) { writer.ok = false; break; }
            snprintf(prefix, sizeof(prefix), " p%u=", unsigned(i + 1));
            writer.text(prefix);
            writer.param(param);
        }
        return writer.finish();
    }
    const char* text = shortText && definition->shortText ? definition->shortText : definition->text;
    while (*text) {
        if (text[0] == '{' && text[1] == 'p') {
            const char* cursor = text + 2;
            unsigned index = 0;
            while (*cursor >= '0' && *cursor <= '9' && index <= LOG_MAX_PARAMS)
                index = index * 10 + (*cursor++ - '0');
            LogParam param;
            if (*cursor != '}' || index == 0 || index > LOG_MAX_PARAMS || !params.get(index - 1, param)) {
                writer.text("<missing>");
                writer.ok = false;
                while (*cursor && *cursor != '}') ++cursor;
                text = *cursor ? cursor + 1 : cursor;
            } else {
                writer.param(param);
                text = cursor + 1;
            }
        } else writer.put(*text++);
    }
    return writer.finish();
}
