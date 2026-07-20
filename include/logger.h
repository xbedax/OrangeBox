#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "config.h"

#ifdef DISP_OLED
#include <Adafruit_GFX.h>
#include <Adafruit_SH1106.h>
#define OLED_ADDRESS 0x3C  //hard codded in Adafruit_SH1106.h SH1106_I2C_ADDRESS

#define OLEDROWS 4
#define OLEDTEXTSIZE 8
#define OLEDLINECHARS 20
#endif


#ifndef LOG_QUEUE_SIZE
#define LOG_QUEUE_SIZE 32
#endif

#ifndef LOG_MESSAGE_MAX_LEN
#define LOG_MESSAGE_MAX_LEN 160
#endif

#ifndef LOG_SOURCE_MAX_LEN
#define LOG_SOURCE_MAX_LEN 16
#endif

#ifndef LOG_RECONNECT_INTERVAL
#define LOG_RECONNECT_INTERVAL 5000
#endif

#define LOGAREA_TEMP    1
#define LOGAREA_COND    2           // box internal environment measurement logging
#define LOGAREA_COMM    3           // communication
#define LOGAREA_SYSTEM  4           //
#define LOGAREA_ACCESS  5           // access control - PIN management

struct LogRecord {
    uint8_t severity;
    uint8_t logArea;
    const char* source;
    const char* message;
    unsigned long timestamp;
};

struct QueuedLogRecord {
    uint8_t severity;
    uint8_t logArea;
    char source[LOG_SOURCE_MAX_LEN + 1];
    char message[LOG_MESSAGE_MAX_LEN + 1];
    unsigned long timestamp;
};

class LogTransport {
public:
    virtual ~LogTransport() = default;

    virtual void begin() = 0;
    virtual void update(unsigned long currentMillis) = 0;
    virtual bool isConnected() const = 0;
    virtual bool sendOrQueue(const LogRecord& record) = 0;
};

class BufferedLogTransport : public LogTransport {
public:
    void begin() override;
    void update(unsigned long currentMillis) override;
    bool sendOrQueue(const LogRecord& record) override;

    uint16_t queuedCount() const;
    uint32_t droppedCount() const;

protected:
    virtual bool reconnect(unsigned long currentMillis) = 0;
    virtual bool sendNow(const QueuedLogRecord& record) = 0;

private:
    QueuedLogRecord queue[LOG_QUEUE_SIZE];
    uint16_t head = 0;
    uint16_t count = 0;
    uint32_t dropped = 0;
    unsigned long nextReconnectMillis = 0;

    QueuedLogRecord makeQueuedRecord(const LogRecord& record) const;
    void queuePush(const LogRecord& record);
    bool queuePop(QueuedLogRecord& record);
    void flushQueue();
    bool sendDroppedNotice();
};

class Logger {
public:
    void begin(LogTransport* remoteTransport = nullptr);
    void setRemoteTransport(LogTransport* remoteTransport);
    void update(unsigned long currentMillis);

    void logPrint(uint8_t severity, const char* message, const char* source = nullptr, uint8_t logArea = LOGAREA_SYSTEM);
    void logPrint(uint8_t severity, const String& message, const char* source = nullptr, uint8_t logArea = LOGAREA_SYSTEM);

private:
    LogTransport* transport = nullptr;

    bool shouldLog(uint8_t severity) const;
    const char* severityName(uint8_t severity) const;
    void writeSerial(const LogRecord& record);
    void writeRemote(const LogRecord& record);
    void DebugPrint(const char* message, const char* source, uint8_t logArea);
};

extern Logger logger;

#endif // LOGGER_H
