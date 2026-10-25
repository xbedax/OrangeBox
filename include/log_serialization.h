#pragma once
#include "logger.h"

// Dictionary-independent JSON serialization. False on malformed data or overflow.
bool serializeLogRecord(const QueuedLogRecord& record, char* output, size_t capacity, size_t& written);
