#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.h"
#include "logger.h"

#ifndef DIAGNOSTICS_MAX_METRICS
#define DIAGNOSTICS_MAX_METRICS 16
#endif

#ifndef DIAGNOSTICS_SNAPSHOT_COUNT
#define DIAGNOSTICS_SNAPSHOT_COUNT 16
#endif

enum class DiagnosticFormat : uint8_t {
    Text,
    Html
};

struct DiagnosticMetric {
    const char* key;
    const char* label;
    uint32_t value;
    const char* unit;
};

struct DiagnosticSnapshot {
    unsigned long timestampMillis;
    uint8_t metricCount;
    DiagnosticMetric metrics[DIAGNOSTICS_MAX_METRICS];
};

class BoxDiagnostics {
public:
    void begin(TaskHandle_t taskToWatch = nullptr);

    bool collectStatistics(DiagnosticSnapshot& snapshot) const;
    void collectStatistics(JsonDocument& doc) const;

    String showStatistics(DiagnosticFormat format = DiagnosticFormat::Text) const;
    bool snapshotStatistics(const char* source = BOX_HOST_NAME, uint8_t logArea = LOGAREA_SYSTEM);
    String showSnapshots(DiagnosticFormat format = DiagnosticFormat::Text, uint8_t maxSnapshots = 0) const;
    String statisticsJson() const;
    String snapshotsJson(uint8_t maxSnapshots = 0) const;

    uint8_t snapshotCount() const;
    bool getSnapshot(uint8_t newestOffset, DiagnosticSnapshot& snapshot) const;

private:
    TaskHandle_t watchedTask = nullptr;
    char watchedTaskName[configMAX_TASK_NAME_LEN + 1] = {};
    DiagnosticSnapshot snapshots[DIAGNOSTICS_SNAPSHOT_COUNT] = {};
    uint8_t nextSnapshot = 0;
    uint8_t storedSnapshots = 0;

    bool addMetric(DiagnosticSnapshot& snapshot,
                   const char* key,
                   const char* label,
                   uint32_t value,
                   const char* unit) const;
    uint32_t metricValue(const DiagnosticSnapshot& snapshot, const char* key) const;
    uint8_t snapshotIndexFromNewest(uint8_t newestOffset) const;
    void storeSnapshot(const DiagnosticSnapshot& snapshot);
    void snapshotToJsonObject(JsonObject target, const DiagnosticSnapshot& snapshot) const;
    String snapshotToJson(const DiagnosticSnapshot& snapshot) const;
    String snapshotToCompactLogJson(const DiagnosticSnapshot& snapshot) const;
    String renderSnapshot(const DiagnosticSnapshot& snapshot,
                          DiagnosticFormat format,
                          const char* title) const;
    String renderEmptySnapshots(DiagnosticFormat format) const;
    String formatMetricValue(const DiagnosticMetric& metric) const;
    String formatBytes(uint32_t bytes) const;
};

extern BoxDiagnostics boxDiagnostics;

#endif // DIAGNOSTICS_H
