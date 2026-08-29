#include "logger.h"
#include <cstdio>
#include <cstring>


#ifdef DISP_OLED
Adafruit_SH1106 display(OLED_ADDRESS);
String logRows[OLEDROWS];
#endif


Logger logger;

void BufferedLogTransport::begin()
{
    head = 0;
    count = 0;
    dropped = 0;
    nextReconnectMillis = 0;
}

void BufferedLogTransport::update(unsigned long currentMillis)
{
    if (!hasPendingOutput()) {
        return;
    }

    if (!isConnected() && currentMillis >= nextReconnectMillis) {
        reconnect(currentMillis);
        nextReconnectMillis = currentMillis + LOG_RECONNECT_INTERVAL;
    }
    if (isConnected()) {
        flushQueue();
    }
}

bool BufferedLogTransport::sendOrQueue(const LogRecord& record)
{
    queuePush(record);
    return false;
}

uint16_t BufferedLogTransport::queuedCount() const
{
    return count;
}

uint32_t BufferedLogTransport::droppedCount() const
{
    return dropped;
}

bool BufferedLogTransport::hasPendingOutput() const
{
    return count > 0 || dropped > 0;
}

QueuedLogRecord BufferedLogTransport::makeQueuedRecord(const LogRecord& record) const
{
    QueuedLogRecord queued = {};
    queued.severity = record.severity;
    queued.logArea = record.logArea;
    queued.timestamp = record.timestamp;

    if (record.source != nullptr) {
        strncpy(queued.source, record.source, LOG_SOURCE_MAX_LEN);
        queued.source[LOG_SOURCE_MAX_LEN] = '\0';
    }

    if (record.message != nullptr) {
        strncpy(queued.message, record.message, LOG_MESSAGE_MAX_LEN);
        queued.message[LOG_MESSAGE_MAX_LEN] = '\0';
    }

    return queued;
}

void BufferedLogTransport::queuePush(const LogRecord& record)
{
    if (count == LOG_QUEUE_SIZE) {
        head = (head + 1) % LOG_QUEUE_SIZE;
        count--;
        dropped++;
    }

    uint16_t tail = (head + count) % LOG_QUEUE_SIZE;
    queue[tail] = makeQueuedRecord(record);
    count++;
}

bool BufferedLogTransport::queuePop(QueuedLogRecord& record)
{
    if (count == 0) {
        return false;
    }

    record = queue[head];
    head = (head + 1) % LOG_QUEUE_SIZE;
    count--;
    return true;
}

void BufferedLogTransport::flushQueue()
{
    if (!sendDroppedNotice()) {
        return;
    }

    while (isConnected() && count > 0) {
        QueuedLogRecord record = {};
        if (!queuePop(record)) {
            return;
        }

        if (!sendNow(record)) {
            LogRecord retry = {
                record.severity,
                record.logArea,
                record.source,
                record.message,
                record.timestamp
            };
            queuePush(retry);
            return;
        }
    }
}

bool BufferedLogTransport::sendDroppedNotice()
{
    if (dropped == 0) {
        return true;
    }

    QueuedLogRecord notice = {};
    notice.severity = SEVERITY_WARNING;
    notice.logArea = LOGAREA_SYSTEM;
    strncpy(notice.source, "logger", LOG_SOURCE_MAX_LEN);
    notice.source[LOG_SOURCE_MAX_LEN] = '\0';
    snprintf(notice.message, sizeof(notice.message),
             "Dropped %lu log message(s) while remote logging was unavailable",
             static_cast<unsigned long>(dropped));
    notice.timestamp = millis();

    if (!sendNow(notice)) {
        return false;
    }

    dropped = 0;
    return true;
}

void Logger::begin(LogTransport* remoteTransport)
{
    setRemoteTransport(remoteTransport);

#ifdef DISP_OLED
//  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  display.begin(SH1106_SWITCHCAPVCC, OLED_ADDRESS);
  display.clearDisplay();
//  display.display();
  display.setTextSize(OLEDTEXTSIZE);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
#endif
}

void Logger::setRemoteTransport(LogTransport* remoteTransport)
{
    transport = remoteTransport;
    if (transport != nullptr) {
        transport->begin();
    }
}

void Logger::update(unsigned long currentMillis)
{
    if (transport != nullptr) {
        transport->update(currentMillis);
    }
}

void Logger::DebugPrint(const char* message, const char* source, uint8_t logArea)
{
#ifdef DISP_OLED
    display.setTextSize(1);
    display.setTextColor(WHITE);
    for (int i = 0; i < OLEDROWS - 1; i++){
        logRows[i] = logRows[i+1];
    }
    logRows[OLEDROWS - 1] = String(message).substring(0, OLEDLINECHARS);
    display.clearDisplay();
    for (int i = 0; i < OLEDROWS; i++){
        display.setCursor(0,(i+1)*OLEDTEXTSIZE);
        display.print(logRows[i]);
    }
    display.display();
#endif
}

void Logger::logPrint(uint8_t severity, const char* message, const char* source, uint8_t logArea)
{
    if (!shouldLog(severity)) {
        return;
    }

    LogRecord record = {
        severity,
        logArea,
        source,
        message,
        millis()
    };

    writeSerial(record);
    writeRemote(record);
    DebugPrint(message, source, logArea);
}

void Logger::logPrint(uint8_t severity, const String& message, const char* source, uint8_t logArea)
{
    logPrint(severity, message.c_str(), source, logArea);
}

bool Logger::shouldLog(uint8_t severity) const
{
    return severity <= LOGPRINT_SEVERITY_LEVEL;
}

const char* Logger::severityName(uint8_t severity) const
{
    switch (severity) {
        case SEVERITY_BUSSINESS:
            return "BUSINESS";
        case SEVERITY_INFO:
            return "INFO";
        case SEVERITY_ERROR:
            return "ERROR";
        case SEVERITY_WARNING:
            return "WARNING";
        case SEVERITY_DEBUG:
            return "DEBUG";
        default:
            return "UNKNOWN";
    }
}

void Logger::writeSerial(const LogRecord& record)
{
    Serial.print('[');
    Serial.print(record.timestamp);
    Serial.print("] ");
    Serial.print(severityName(record.severity));
    Serial.print(" area=");
    Serial.print(record.logArea);

    if (record.source != nullptr && record.source[0] != '\0') {
        Serial.print(" ");
        Serial.print(record.source);
    }

    Serial.print(": ");
    if (record.message != nullptr) {
        Serial.print(record.message);
    }
}

void Logger::writeRemote(const LogRecord& record)
{
    if (transport != nullptr) {
        transport->sendOrQueue(record);
    }
}
