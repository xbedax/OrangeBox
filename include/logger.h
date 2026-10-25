#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "config.h"
#include <type_traits>
#include <atomic>
#include <utility>
#include <ctime>

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

#ifndef LOG_SOURCE_MAX_LEN
#define LOG_SOURCE_MAX_LEN 16
#endif

#ifndef LOG_RECONNECT_INTERVAL
#define LOG_RECONNECT_INTERVAL 5000
#endif

/* Log Area definitions */
#define LOGAREA_COND        2           // box internal environment measurement logging
#define LOGAREA_COMM        3           // communication
#define LOGAREA_SYSTEM      4           //
#define LOGAREA_ACCESS      5           // access control - PIN management
#define LOGAREA_OPERATION   6           // bussiness operations (door open/closed, picture taken, code/pin used,)

// Stable numeric message IDs. Never reuse retired IDs.
#define A_INVALID_ACTION_TYPE                                            1
#define A_INVALID_INFO_TYPE                                              3
#define A_INVALID_RESPONSE_TYPE                                          4
#define A_DISPLAY_INITIALIZING                                           5
#define A_DIAGNOSTIC_SNAPSHOT                                            19
#define A_DOOR_STATE_READ                                                20
#define A_INVALID_LOCK_DOOR                                              21
#define A_GPIO_DOORS_LOADED                                              22
#define A_GPIO_DOOR_MAPPING                                              23
#define A_INVALID_KEYBOARD_MODE                                          32
#define A_MISSING_DOOR_NUMBER                                            44
#define A_INVALID_DOOR_NUMBER_TEXT                                       46
#define A_INVALID_DOOR_FORMAT                                            47
#define A_INVALID_DOOR_NUMBER                                            48
#define A_DOOR_STATE_REQUEST                                             49
#define A_DOOR_MIXED_STATE                                               50
#define A_PRESENCE_CODE_REQUIRED                                         53
#define A_PRESENCE_CODE_INVALID                                          54
#define A_DOOR_OPEN_QUEUED                                               55
#define A_VPN_BEGIN_FAILED                                               56
#define A_VPN_CONFIGURED                                                 57
#define A_VPN_SCHEDULED                                                  58
#define A_RESTART                                                        59
#define A_RTC_OSF                                                        60
#define A_SYSTEM_TIME_LOADED_FROM_RTC_WITH_OSF_SET                       62
#define A_SYSTEM_TIME_LOADED_FROM_RTC                                    63
#define A_RTC_AVAILABLE_BUT_TIME_IS_NOT_TRUSTED_YET                      64
#define A_RTC_NOT_FOUND_AT_X                                             65
#define A_CURRENT_EPOCH                                                  66
#define A_CURRENT_TIME                                                   68
#define A_DISPLAY_INITIALIZED                                            70
#define A_BOX_PROTOTYPE                                                  71
#define A_HAL_INITIALIZED                                                72
#define A_KEYBOARD_INITIALIZED                                           73
#define A_SCANNER_COMMAND_CONFIGURATION_IS_INVALID                       74
#define A_STATE_MACHINE_INITIALIZED                                      75
#define A_STARTING_WIFI                                                  78
#define A_WIFI_CONNECTING                                                79
#define A_WIFI_CONNECTED                                                 80
#define A_WIFI_ADDRESS                                                   81
#define A_WEB_URL                                                        83
#define A_WIFI_FAILED                                                    85
#define A_NTP_SYNCHRONIZATION_SCHEDULED                                  86
#define A_NTP_SYNCHRONIZATION_NOT_CONFIGURED                             87
#define A_CREDENTIAL_STORAGE_INIT                                        88
#define A_WEBSOCKET_INITIALIZED                                          92
#define A_WEBSERVER_STARTED                                              93
#define A_SETUP_COMPLETE                                                 94
#define A_DOOR_MIXED_NOTIFICATION                                        95
#define A_INVALID_DOOR_NUMBER_IN_NOTIFICATION                            96
#define A_LOCK_TIMER_PAYLOAD                                             97
#define A_LOCK_DEACTIVATED                                               98
#define A_INVALID_LOCK_NUMBER_IN_TIMER_CALLBACK                          99
#define A_DEACTIVATING_AMBIENT_LIGHT                                     100
#define A_PASSWORD_TIMEOUT_PASSED                                        101
#define A_UNKNOWN_DUE_ACTION_CALLED                                      102
#define A_WIFI_LOST                                                      103
#define A_RESTARTING_WIFI_ASSOCIATION                                    104
#define A_WIFI_RESTORED                                                  105
#define A_CREDSTORE_UPDATECRED_INVALID_CREDENTIAL                        106
#define A_CREDSTORE_ADDCRED_INVALID_CREDENTIAL                           107
#define A_CREDSTORE_ADDCRED_FAILED_TO_WRITE_NEW_CREDENTIAL               108
#define A_CREDENTIAL_VERIFY_STARTED                                      109
#define A_CREDENTIAL_VERIFY_CANDIDATE                                    110
#define A_CREDSTORE_REMOVECRED_RECORD_WITH_CREDID_NOT_FOUND              111
#define A_CREDSTORE_UPDATE_CREDID_NOT_PROPERLY_CHAINED                   112
#define A_CREDSTORE_REMOVECRED_NAME_OR_CREDENTIAL_MISMATCH_FOR_CREDID    113
#define A_CREDSTORE_REMOVECRED_FAILED_TO_READ_PREV_RECORD_WITH           114
#define A_CREDSTORE_REMOVECRED_FAILED_TO_READ_NEXT_RECORD_WITH           115
#define A_CREDSTORE_UPDATE_RECORD_WITH_CREDID_NOT_FOUND                  116
#define A_CREDSTORE_UPDATE_MISMATCH                                      117
#define A_CREDSTORE_UPDATE_WRITE_FAILED_FOR_CREDID                       118
#define A_CREDENTIAL_WRITE                                               119
#define A_CREDSTORE_WRITECRED_FAILED_TO_READ_LAST_RECORD                 120
#define A_CREDSTORE_WRITECRED_FAILED_TO_READ_PREV_RECORD_WITH            121
#define A_CREDSTORE_UPDATE_FAILED_TO_WRITE_CREDID                        122
#define A_CREDSTORE_UPDATE_PARTIAL_WRITE_FOR_CREDID_WRITTEN_OF           123
#define A_CREDSTORE_REBUILDCACHE_HEAD_UNREADABLE                         127
#define A_CREDSTORE_REBUILDCACHE_BROKEN_LINK_AT_ID                       128
#define A_CREDSTORE_CACHE_ACTIVE_RECORD_S_MAXCREDID                      129
#define A_CREDSTORE_RESETTING_LEGACY_TEST_DATA_POOL                      130
#define A_CREDSTORE_GC_EMPTY_CREDENTIAL_POOL_CREATING_SENTINELS          131
#define A_CREDSTORE_GC_CORRUPTED_SENTINELS_RESETTING_CREDENTIAL_POOL     132
#define A_CREDSTORE_GC_ROLLED_BACK_INTERRUPTED_INSERT_TAILPREV_NEXTID    133
#define A_CREDSTORE_GC_FOUND_INTERRUPTED_INSERT_TAILPREV_NEXTID          134
#define A_CREDSTORE_GC_TANGLED_CHAIN_RESETTING_CREDENTIAL_POOL           135
#define A_CREDSTORE_GC_CORRUPTED_DELETE_RECORD_CREDID_RESETTING_CREDENTIAL 136
#define A_CREDSTORE_GC_REMOVED_INTERRUPTED_DELETE_RECORD_CREDID          137
#define A_CREDSTORE_GC_COMPLETED_INTERRUPTED_DELETE_AFTER_REMOVED_CREDID 138
#define A_CREDSTORE_GC_TRAVERSING_CHAIN_CURRENT_CREDID_NEXTID            139
#define A_CREDSTORE_GC_ROLLED_BACK_BROKEN_INSERT                         140
#define A_CREDSTORE_GC_INTERRUPTED_CHAIN_RESETTING_CREDENTIAL_POOL       141
#define A_CREDSTORE_GC_UNEXPECTED_ID_IN_CHAIN_RESETTING_CREDENTIAL       142
#define A_CREDSTORE_GC_DUBIOUS_RECORD_IN_CHAIN_RESETTING_CREDENTIAL      143
#define A_CREDSTORE_GC_REDOING_UNFINISHED_INSERT_CREDID                  144
#define A_CREDSTORE_GC_COMPLETED_INTERRUPTED_DELETE                      146
#define A_CREDSTORE_GC_BROKEN_CHAIN_RESETTING_CREDENTIAL_POOL            147
#define A_CREDSTORE_GC_UNABLE_TO_TRAVERSE_CHAIN_RESETTING_POOL           148
#define A_CREDSTORE_GC_UNRECOVERABLE_CHAIN_STATE_RESETTING_CREDENTIAL_POOL 149
#define A_CREDSTORE_GC_COMPLETED                                         152
#define A_DOOR_OPEN_TIMEOUT                                              153
#define A_KEYBOARD_EVENT                                                 154
#define A_SENSOR_EVENT                                                   155
#define A_DOOR_OPEN_FAILED                                               156
#define A_CREDENTIAL_CONSUME_FAILED                                      157
#define A_REGISTERING_COMMAND_HANDLER_FOR_COMMAND                        158
#define A_FAILED_TO_PARSE_JSON                                           159
#define A_INVALID_MESSAGE_ENVELOPE                                       160
#define A_RECEIVED_COMMAND                                               161
#define A_FOUND_HANDLER_FOR_COMMAND                                      162
#define A_RECEIVED_MESSAGE_WITH_UNKNOWN_COMMAND                          163
#define A_CLIENT_CONNECTED                                               164
#define A_CLIENT_DISCONNECTED                                            165
#define A_INVALID_CLIENT_INDEX_FOR_WATCHDOG_REMOVAL                      167
#define A_REMOVED_WATCHDOG_ENTRY_FOR_CLIENT                              168
#define A_WATCHDOG_LIMIT                                                 169
#define A_CLIENT_TIMEOUT                                                 170
#define A_DOOR_OPENED                                                    171
#define A_DOOR_CLOSED                                                    172
#define A_PASSWORD_STARTED                                               173
#define A_PASSWORD_CANCELED                                              174
#define A_PASSWORD_ENTERED                                               175
#define A_PASSWORD_VERIFIED                                              176
#define A_PASSWORD_BAD                                                   177
#define A_CODE_SCANNED                                                   178
#define A_CODE_VERIFIED                                                  179
#define A_CODE_UNKNOWN                                                   180
#define A_PRESENCE_STARTED                                               181
#define A_PRESENCE_CANCELED                                              182
#define A_OPEN_COMMAND_RECEIVED                                          183
#define A_PIN_EDIT                                                       184
#define A_PIN_EDIT_ERROR                                                 185
#define A_CODE_EDIT                                                      186
#define A_CODE_EDIT_ERROR                                                187
#define A_VPN_CONNECTED                                                  188
#define A_VPN_DISCONNECTED                                               189
#define A_LOG_DROPPED                                                    190
#define A_LOG_REJECTED                                                   191
#define A_DOOR_QUEUE_FAILED                                              192
#define A_STATE_CHANGED                                                  193
#define A_TIMER_EVENT                                                    194
#define A_PASSWORD_TIMEOUT                                               195
#define A_PRESENCE_TIMEOUT                                               196
#define A_SCAN_STARTED                                                   197
#define A_SCAN_FAILED                                                    198
#define A_STATE_READ                                                     199
#define A_CREDENTIAL_INPUT_INVALID                                       200
#define A_SCANNER_BUFFER_OVERFLOW                                        201
#define A_NVS_OPERATION_FAILED                                           202
#define A_STORED_CREDENTIAL_INVALID                                      203

struct LogMessageDefinition {
    uint16_t id;
    const char* text;
    const char* shortText;
};
const LogMessageDefinition* findLogMessage(uint16_t id);
uint8_t logMessageParamCount(const LogMessageDefinition& definition);

struct LogParam {
    enum class Type : uint8_t { Signed, Unsigned, Real, Boolean, Text };
    Type type;
    union Value {
        int64_t signedValue;
        uint64_t unsignedValue;
        double realValue;
        bool boolValue;
        const char* textValue;
        Value() : unsignedValue(0) {}
    } value;
    LogParam() : type(Type::Unsigned) {}
    LogParam(const char* text) : type(Type::Text) { value.textValue = text ? text : ""; }
    LogParam(const String& text) : LogParam(text.c_str()) {}
    LogParam(bool v) : type(Type::Boolean) { value.boolValue = v; }
    template<typename T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value, int>::type = 0>
    LogParam(T v) : type(std::is_signed<T>::value ? Type::Signed : Type::Unsigned) {
        if constexpr (std::is_signed<T>::value) value.signedValue = static_cast<int64_t>(v);
        else value.unsignedValue = static_cast<uint64_t>(v);
    }
    template<typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    LogParam(T v) : type(Type::Real) { value.realValue = static_cast<double>(v); }
};

// Copyable, owning TLV block. Text includes its terminating zero; no pointers
// into callers or into this block are stored in the queue.
struct LogParamBlock {
    uint8_t count = 0;
    uint16_t used = 0;
    uint8_t data[LOG_PARAM_DATA_MAX_LEN] = {};
    bool append(const LogParam& param);
    bool get(uint8_t index, LogParam& param) const;
};

struct LogRecord {
    uint8_t severity;
    uint8_t logArea;
    const char* source;
    uint16_t message;
    unsigned long timestamp; // epoch seconds, not millis() uptime
};
struct LogEvent {
    LogRecord record;
    const LogParam* params;
    uint8_t paramCount;
};
struct QueuedLogRecord {
    uint8_t severity;
    uint8_t logArea;
    char source[LOG_SOURCE_MAX_LEN + 1];
    uint16_t message;
    unsigned long timestamp;
    LogParamBlock params;
    uint32_t sequence;
};

// Returns false on truncation or malformed/missing placeholders.
// Unknown IDs use a fallback containing A<ID> and all parameters.
bool formatLogMessage(uint16_t id, const LogParamBlock& params, bool shortText,
                      char* output, size_t capacity);

class LogTransport {
public:
    virtual ~LogTransport() = default;
    virtual void begin() = 0;
    virtual void update(unsigned long currentMillis) = 0;
    virtual bool isConnected() const = 0;
    // Copies parameters before returning; true means accepted, not delivered.
    virtual bool sendOrQueue(const LogEvent& event) = 0;
};

class BufferedLogTransport : public LogTransport {
public:
    void begin() override;
    void update(unsigned long currentMillis) override;
    bool sendOrQueue(const LogEvent& event) override;
    uint16_t queuedCount() const;
    uint32_t droppedCount() const;
    bool hasPendingOutput() const;
protected:
    virtual bool reconnect(unsigned long currentMillis) = 0;
    virtual bool sendNow(const QueuedLogRecord& record) = 0;
private:
    QueuedLogRecord queue[LOG_QUEUE_SIZE];
    uint16_t head = 0;
    uint16_t count = 0;
    uint32_t dropped = 0;
    uint32_t nextSequence = 0;
    unsigned long reconnectStartedAt = 0;
    bool reconnectStarted = false;
#ifdef ARDUINO_ARCH_ESP32
    mutable portMUX_TYPE queueMux = portMUX_INITIALIZER_UNLOCKED;
#endif
    void lock() const;
    void unlock() const;
    void flushQueue();
    bool sendDroppedNotice();
};

class Logger {
public:
    void begin(LogTransport* remoteTransport = nullptr, const char* source = BOX_HOST_NAME);
    void setRemoteTransport(LogTransport* remoteTransport);
    void update(unsigned long currentMillis);
    uint32_t rejectedCount() const { return rejected.load(); }

    template<typename... Args>
    void logPrint(uint16_t messageId, uint8_t area, uint8_t severity, Args&&... args) {
        logPrintFrom(localSource[0] ? localSource : BOX_HOST_NAME,
                     messageId, area, severity, std::forward<Args>(args)...);
    }
    // For diagnostic callers that explicitly supply an event source.
    template<typename... Args>
    void logPrintFrom(const char* source, uint16_t messageId, uint8_t area,
                      uint8_t severity, Args&&... args) {
        static_assert(sizeof...(Args) <= LOG_MAX_PARAMS, "Too many log parameters");
        if (!shouldLog(severity)) return;
        const LogParam params[] = {LogParam(std::forward<Args>(args))..., LogParam()};
        emit({{severity, area, source, messageId, static_cast<unsigned long>(time(nullptr))},
              params, static_cast<uint8_t>(sizeof...(Args))});
    }
private:
    LogTransport* transport = nullptr;
    char localSource[LOG_SOURCE_MAX_LEN + 1] = {};
    std::atomic<uint32_t> rejected{0};
    bool shouldLog(uint8_t severity) const;
    const char* severityName(uint8_t severity) const;
    void emit(const LogEvent& event);
    void reject(uint16_t messageId, const char* reason);
    void writeSerial(const LogRecord& record, const LogParamBlock& params);
    void writeRemote(const LogEvent& event);
    void writeDebug(const LogRecord& record, const LogParamBlock& params);
};

extern Logger logger;
#endif // LOGGER_H
