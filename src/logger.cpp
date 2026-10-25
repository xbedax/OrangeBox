#include "logger.h"
#include "timeout.h"
#include <cstdio>
#include <cstring>

#ifdef DISP_OLED
Adafruit_SH1106 display(OLED_ADDRESS);
String logRows[OLEDROWS];
#endif

Logger logger;

void BufferedLogTransport::lock() const {
#ifdef ARDUINO_ARCH_ESP32
    portENTER_CRITICAL(&queueMux);
#endif
}
void BufferedLogTransport::unlock() const {
#ifdef ARDUINO_ARCH_ESP32
    portEXIT_CRITICAL(&queueMux);
#endif
}

void BufferedLogTransport::begin() {
    lock();
    head = count = 0;
    dropped = nextSequence = 0;
    unlock();
    reconnectStarted = false;
}

bool BufferedLogTransport::sendOrQueue(const LogEvent& event) {
    QueuedLogRecord queued = {};
    queued.severity = event.record.severity;
    queued.logArea = event.record.logArea;
    queued.message = event.record.message;
    queued.timestamp = event.record.timestamp;
    const char* source = event.record.source ? event.record.source : "";
    if (strlen(source) > LOG_SOURCE_MAX_LEN || event.paramCount > LOG_MAX_PARAMS
        || (event.paramCount && !event.params)) return false;
    strcpy(queued.source, source);
    for (uint8_t i = 0; i < event.paramCount; ++i)
        if (!queued.params.append(event.params[i])) return false;

    lock();
    if (count == LOG_QUEUE_SIZE) {
        head = (head + 1) % LOG_QUEUE_SIZE;
        --count;
        ++dropped;
    }
    queued.sequence = nextSequence++;
    queue[(head + count) % LOG_QUEUE_SIZE] = queued;
    ++count;
    unlock();
    return true;
}

uint16_t BufferedLogTransport::queuedCount() const {
    lock(); const auto value = count; unlock(); return value;
}
uint32_t BufferedLogTransport::droppedCount() const {
    lock(); const auto value = dropped; unlock(); return value;
}
bool BufferedLogTransport::hasPendingOutput() const {
    lock(); const bool pending = count || dropped; unlock(); return pending;
}

void BufferedLogTransport::update(unsigned long now) {
    if (!hasPendingOutput()) return;
    if (!isConnected() && (!reconnectStarted || timeoutElapsed(reconnectStartedAt, LOG_RECONNECT_INTERVAL, now))) {
        reconnect(now);
        reconnectStartedAt = now;
        reconnectStarted = true;
    }
    if (isConnected()) flushQueue();
}

bool BufferedLogTransport::sendDroppedNotice() {
    lock(); const uint32_t lost = dropped; unlock();
    if (!lost) return true;
    QueuedLogRecord notice = {};
    notice.severity = SEVERITY_ERROR;
    notice.logArea = LOGAREA_SYSTEM;
    strcpy(notice.source, "logger");
    notice.message = A_LOG_DROPPED;
    notice.timestamp = static_cast<unsigned long>(time(nullptr));
    if (!notice.params.append(LogParam(lost)) || !sendNow(notice)) return false;
    lock(); dropped -= lost; unlock();
    return true;
}

void BufferedLogTransport::flushQueue() {
    if (!sendDroppedNotice()) return;
    // Bound one update even if another task continuously produces log events.
    for (uint16_t budget = 0; budget < LOG_QUEUE_SIZE && isConnected(); ++budget) {
        QueuedLogRecord record = {};
        lock();
        const bool available = count != 0;
        if (available) record = queue[head];
        unlock();
        if (!available || !sendNow(record)) return;
        // A concurrent producer may have overwritten this head while sending.
        lock();
        if (count && queue[head].sequence == record.sequence) {
            head = (head + 1) % LOG_QUEUE_SIZE;
            --count;
        }
        unlock();
    }
}

void Logger::begin(LogTransport* remoteTransport, const char* source) {
    if (source && strlen(source) <= LOG_SOURCE_MAX_LEN) strcpy(localSource, source);
    setRemoteTransport(remoteTransport);
#ifdef DISP_OLED
    display.begin(SH1106_SWITCHCAPVCC, OLED_ADDRESS);
    display.clearDisplay();
    display.setTextSize(OLEDTEXTSIZE);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
#endif
}

void Logger::setRemoteTransport(LogTransport* remoteTransport) {
    transport = remoteTransport;
    if (transport) transport->begin();
}
void Logger::update(unsigned long now) {
    if (transport) transport->update(now);
}

bool Logger::shouldLog(uint8_t severity) const {
    return severity >= LOGPRINT_MIN_SEVERITY && severity >= SEVERITY_DEBUG && severity <= SEVERITY_CRITICAL;
}

const char* Logger::severityName(uint8_t severity) const {
    switch (severity) {
        case SEVERITY_DEBUG: return "DEBUG";
        case SEVERITY_INFO: return "INFO";
        case SEVERITY_WARNING: return "WARNING";
        case SEVERITY_ERROR: return "ERROR";
        case SEVERITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

void Logger::reject(uint16_t messageId, const char* reason) {
    ++rejected;
    // Never recurse through the failing event. This fixed, small event fits.
    if (messageId == A_LOG_REJECTED || !shouldLog(SEVERITY_ERROR)) return;
    const LogParam params[] = {LogParam(messageId), LogParam(reason)};
    emit({{SEVERITY_ERROR, LOGAREA_SYSTEM, "logger", A_LOG_REJECTED,
           static_cast<unsigned long>(time(nullptr))}, params, 2});
}

void Logger::emit(const LogEvent& event) {
    if (event.paramCount > LOG_MAX_PARAMS || (event.paramCount && !event.params)) {
        reject(event.record.message, "parameter count"); return;
    }
    if (event.record.source && strlen(event.record.source) > LOG_SOURCE_MAX_LEN) {
        reject(event.record.message, "source length"); return;
    }
    const auto* definition = findLogMessage(event.record.message);
    if (definition && logMessageParamCount(*definition) != event.paramCount) {
        reject(event.record.message, "placeholder count"); return;
    }
    LogParamBlock block;
    for (uint8_t i = 0; i < event.paramCount; ++i) {
        if (!block.append(event.params[i])) {
            reject(event.record.message, "parameter block"); return;
        }
    }
    writeSerial(event.record, block);
    writeDebug(event.record, block);
    writeRemote(event);
}

void Logger::writeSerial(const LogRecord& record, const LogParamBlock& params) {
    // Large enough for twelve 64-bit values plus the longest dictionary text.
    char message[768];
    const bool complete = formatLogMessage(record.message, params, false, message, sizeof(message));
    Serial.printf("[%lu] %s area=%u", record.timestamp, severityName(record.severity), unsigned(record.logArea));
    if (record.source && record.source[0]) {
        Serial.print(" ");
        Serial.print(record.source);
    }
    Serial.print(": ");
    Serial.print(message);
    if (!complete) Serial.print(" [format incomplete]");
    Serial.println();
}

void Logger::writeDebug(const LogRecord& record, const LogParamBlock& params) {
#ifdef DISP_OLED
    char message[768];
    formatLogMessage(record.message, params, true, message, sizeof(message));
    display.setTextSize(1);
    display.setTextColor(WHITE);
    for (int i = 0; i < OLEDROWS - 1; ++i) logRows[i] = logRows[i + 1];
    logRows[OLEDROWS - 1] = String(message).substring(0, OLEDLINECHARS);
    display.clearDisplay();
    for (int i = 0; i < OLEDROWS; ++i) {
        display.setCursor(0, (i + 1) * OLEDTEXTSIZE);
        display.print(logRows[i]);
    }
    display.display();
#else
    (void)record; (void)params;
#endif
}

void Logger::writeRemote(const LogEvent& event) {
    if (transport && !transport->sendOrQueue(event)) reject(event.record.message, "transport rejected");
}
