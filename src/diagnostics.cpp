#include "diagnostics.h"

#include <cstring>
#include <cstdio>
#include "logger.h"

namespace {
const char* KEY_HEAP_INTERNAL_FREE = "heap_internal_free_bytes";
const char* KEY_HEAP_INTERNAL_MAX_ALLOC = "heap_internal_max_alloc_bytes";
const char* KEY_HEAP_INTERNAL_MIN_FREE = "heap_internal_min_free_bytes";
const char* KEY_PSRAM_FREE = "psram_free_bytes";
const char* KEY_PSRAM_MAX_ALLOC = "psram_max_alloc_bytes";
const char* KEY_PSRAM_MIN_FREE = "psram_min_free_bytes";
const char* KEY_TASK_STACK_HWM = "task_stack_high_water_mark_bytes";
const char* KEY_FLASH_CHIP_SIZE = "flash_chip_size_bytes";
const char* KEY_FLASH_SKETCH_SIZE = "flash_sketch_size_bytes";
const char* KEY_FLASH_FREE_SKETCH_SPACE = "flash_free_sketch_space_bytes";
}

BoxDiagnostics boxDiagnostics;

void BoxDiagnostics::begin(TaskHandle_t taskToWatch)
{
    watchedTask = taskToWatch != nullptr ? taskToWatch : xTaskGetCurrentTaskHandle();

    const char* name = watchedTask != nullptr ? pcTaskGetName(watchedTask) : nullptr;
    if (name != nullptr) {
        strncpy(watchedTaskName, name, configMAX_TASK_NAME_LEN);
        watchedTaskName[configMAX_TASK_NAME_LEN] = '\0';
    } else {
        strncpy(watchedTaskName, "current", configMAX_TASK_NAME_LEN);
        watchedTaskName[configMAX_TASK_NAME_LEN] = '\0';
    }
}

bool BoxDiagnostics::collectStatistics(DiagnosticSnapshot& snapshot) const
{
    snapshot = {};
    snapshot.timestampMillis = millis();

    addMetric(snapshot, "uptime_ms", "Uptime", snapshot.timestampMillis, "ms");
    addMetric(snapshot, "heap_internal_total_bytes", "Internal heap total", ESP.getHeapSize(), "B");
    addMetric(snapshot, KEY_HEAP_INTERNAL_FREE, "Internal heap free", ESP.getFreeHeap(), "B");
    addMetric(snapshot, KEY_HEAP_INTERNAL_MAX_ALLOC, "Internal heap max alloc block", ESP.getMaxAllocHeap(), "B");
    addMetric(snapshot, KEY_HEAP_INTERNAL_MIN_FREE, "Internal heap min free", ESP.getMinFreeHeap(), "B");
    addMetric(snapshot, "psram_total_bytes", "PSRAM heap total", ESP.getPsramSize(), "B");
    addMetric(snapshot, KEY_PSRAM_FREE, "PSRAM heap free", ESP.getFreePsram(), "B");
    addMetric(snapshot, KEY_PSRAM_MAX_ALLOC, "PSRAM heap max alloc block", ESP.getMaxAllocPsram(), "B");
    addMetric(snapshot, KEY_PSRAM_MIN_FREE, "PSRAM heap min free", ESP.getMinFreePsram(), "B");

    TaskHandle_t task = watchedTask != nullptr ? watchedTask : xTaskGetCurrentTaskHandle();
    addMetric(snapshot,
              KEY_TASK_STACK_HWM,
              "Watched task stack high water mark",
              static_cast<uint32_t>(uxTaskGetStackHighWaterMark(task)),
              "B");
    addMetric(snapshot, "task_count", "FreeRTOS task count", static_cast<uint32_t>(uxTaskGetNumberOfTasks()), "");

    addMetric(snapshot, KEY_FLASH_CHIP_SIZE, "Flash chip size", ESP.getFlashChipSize(), "B");
    addMetric(snapshot, KEY_FLASH_SKETCH_SIZE, "Current sketch size", ESP.getSketchSize(), "B");
    addMetric(snapshot, KEY_FLASH_FREE_SKETCH_SPACE, "OTA free sketch space", ESP.getFreeSketchSpace(), "B");
    addMetric(snapshot, "flash_chip_speed_hz", "Flash chip speed", ESP.getFlashChipSpeed(), "Hz");

    return snapshot.metricCount > 0;
}

void BoxDiagnostics::collectStatistics(JsonDocument& doc) const
{
    DiagnosticSnapshot snapshot = {};
    collectStatistics(snapshot);
    snapshotToJsonObject(doc.to<JsonObject>(), snapshot);
}

String BoxDiagnostics::showStatistics(DiagnosticFormat format) const
{
    DiagnosticSnapshot snapshot = {};
    collectStatistics(snapshot);
    return renderSnapshot(snapshot, format, "Current diagnostics");
}

bool BoxDiagnostics::snapshotStatistics(const char* source, uint8_t logArea)
{
    DiagnosticSnapshot snapshot = {};
    if (!collectStatistics(snapshot)) {
        return false;
    }

    storeSnapshot(snapshot);
    logger.logPrint(SEVERITY_INFO, snapshotToCompactLogJson(snapshot), source, logArea);
    return true;
}

String BoxDiagnostics::showSnapshots(DiagnosticFormat format, uint8_t maxSnapshots) const
{
    if (storedSnapshots == 0) {
        return renderEmptySnapshots(format);
    }

    uint8_t limit = maxSnapshots == 0 || maxSnapshots > storedSnapshots ? storedSnapshots : maxSnapshots;
    String output;
    output.reserve(limit * 650);

    if (format == DiagnosticFormat::Html) {
        output += "<section class=\"diagnostic-snapshots\"><h2>Diagnostic snapshots</h2>";
    } else {
        output += "Diagnostic snapshots\n";
    }

    for (uint8_t i = 0; i < limit; i++) {
        DiagnosticSnapshot snapshot = {};
        if (!getSnapshot(i, snapshot)) {
            continue;
        }
        String title = "Snapshot ";
        title += String(static_cast<unsigned int>(i + 1));
        title += " @ ";
        title += String(static_cast<unsigned long>(snapshot.timestampMillis));
        title += " ms";
        output += renderSnapshot(snapshot, format, title.c_str());
        if (format == DiagnosticFormat::Text) {
            output += '\n';
        }
    }

    if (format == DiagnosticFormat::Html) {
        output += "</section>";
    }
    return output;
}

String BoxDiagnostics::statisticsJson() const
{
    DiagnosticSnapshot snapshot = {};
    collectStatistics(snapshot);
    return snapshotToJson(snapshot);
}

String BoxDiagnostics::snapshotsJson(uint8_t maxSnapshots) const
{
    JsonDocument doc;
    doc["count"] = storedSnapshots;
    JsonArray items = doc["snapshots"].to<JsonArray>();

    uint8_t limit = maxSnapshots == 0 || maxSnapshots > storedSnapshots ? storedSnapshots : maxSnapshots;
    for (uint8_t i = 0; i < limit; i++) {
        DiagnosticSnapshot snapshot = {};
        if (!getSnapshot(i, snapshot)) {
            continue;
        }
        JsonObject item = items.add<JsonObject>();
        snapshotToJsonObject(item, snapshot);
    }

    String output;
    serializeJson(doc, output);
    return output;
}

uint8_t BoxDiagnostics::snapshotCount() const
{
    return storedSnapshots;
}

bool BoxDiagnostics::getSnapshot(uint8_t newestOffset, DiagnosticSnapshot& snapshot) const
{
    if (newestOffset >= storedSnapshots) {
        return false;
    }

    snapshot = snapshots[snapshotIndexFromNewest(newestOffset)];
    return true;
}

bool BoxDiagnostics::addMetric(DiagnosticSnapshot& snapshot,
                               const char* key,
                               const char* label,
                               uint32_t value,
                               const char* unit) const
{
    if (snapshot.metricCount >= DIAGNOSTICS_MAX_METRICS) {
        return false;
    }

    DiagnosticMetric& metric = snapshot.metrics[snapshot.metricCount++];
    metric.key = key;
    metric.label = label;
    metric.value = value;
    metric.unit = unit;
    return true;
}

uint32_t BoxDiagnostics::metricValue(const DiagnosticSnapshot& snapshot, const char* key) const
{
    for (uint8_t i = 0; i < snapshot.metricCount; i++) {
        if (strcmp(snapshot.metrics[i].key, key) == 0) {
            return snapshot.metrics[i].value;
        }
    }
    return 0;
}

uint8_t BoxDiagnostics::snapshotIndexFromNewest(uint8_t newestOffset) const
{
    return static_cast<uint8_t>((nextSnapshot + DIAGNOSTICS_SNAPSHOT_COUNT - 1 - newestOffset) % DIAGNOSTICS_SNAPSHOT_COUNT);
}

void BoxDiagnostics::storeSnapshot(const DiagnosticSnapshot& snapshot)
{
    snapshots[nextSnapshot] = snapshot;
    nextSnapshot = static_cast<uint8_t>((nextSnapshot + 1) % DIAGNOSTICS_SNAPSHOT_COUNT);
    if (storedSnapshots < DIAGNOSTICS_SNAPSHOT_COUNT) {
        storedSnapshots++;
    }
}

void BoxDiagnostics::snapshotToJsonObject(JsonObject target, const DiagnosticSnapshot& snapshot) const
{
    target["timestamp_ms"] = snapshot.timestampMillis;
    target["watched_task"] = watchedTaskName;

    JsonObject metrics = target["metrics"].to<JsonObject>();
    for (uint8_t i = 0; i < snapshot.metricCount; i++) {
        metrics[snapshot.metrics[i].key] = snapshot.metrics[i].value;
    }
}

String BoxDiagnostics::snapshotToJson(const DiagnosticSnapshot& snapshot) const
{
    JsonDocument doc;
    snapshotToJsonObject(doc.to<JsonObject>(), snapshot);

    String output;
    serializeJson(doc, output);
    return output;
}

String BoxDiagnostics::snapshotToCompactLogJson(const DiagnosticSnapshot& snapshot) const
{
    char output[LOG_MESSAGE_MAX_LEN + 1] = {};
    snprintf(output,
             sizeof(output),
             "{\"t\":%lu,\"ih\":[%lu,%lu,%lu],\"ps\":[%lu,%lu,%lu],\"stk\":%lu,\"fl\":[%lu,%lu,%lu]}",
             static_cast<unsigned long>(snapshot.timestampMillis),
             static_cast<unsigned long>(metricValue(snapshot, KEY_HEAP_INTERNAL_FREE)),
             static_cast<unsigned long>(metricValue(snapshot, KEY_HEAP_INTERNAL_MAX_ALLOC)),
             static_cast<unsigned long>(metricValue(snapshot, KEY_HEAP_INTERNAL_MIN_FREE)),
             static_cast<unsigned long>(metricValue(snapshot, KEY_PSRAM_FREE)),
             static_cast<unsigned long>(metricValue(snapshot, KEY_PSRAM_MAX_ALLOC)),
             static_cast<unsigned long>(metricValue(snapshot, KEY_PSRAM_MIN_FREE)),
             static_cast<unsigned long>(metricValue(snapshot, KEY_TASK_STACK_HWM)),
             static_cast<unsigned long>(metricValue(snapshot, KEY_FLASH_CHIP_SIZE)),
             static_cast<unsigned long>(metricValue(snapshot, KEY_FLASH_SKETCH_SIZE)),
             static_cast<unsigned long>(metricValue(snapshot, KEY_FLASH_FREE_SKETCH_SPACE)));
    return String(output);
}

String BoxDiagnostics::renderSnapshot(const DiagnosticSnapshot& snapshot,
                                      DiagnosticFormat format,
                                      const char* title) const
{
    String output;
    output.reserve(650);

    if (format == DiagnosticFormat::Html) {
        output += "<section class=\"diagnostics\"><h2>";
        output += title;
        output += "</h2><p>Watched task: ";
        output += watchedTaskName;
        output += "</p><table><thead><tr><th>Metric</th><th>Value</th></tr></thead><tbody>";
        for (uint8_t i = 0; i < snapshot.metricCount; i++) {
            output += "<tr><td>";
            output += snapshot.metrics[i].label;
            output += "</td><td><code>";
            output += formatMetricValue(snapshot.metrics[i]);
            output += "</code></td></tr>";
        }
        output += "</tbody></table></section>";
        return output;
    }

    output += title;
    output += '\n';
    output += "Watched task: ";
    output += watchedTaskName;
    output += '\n';
    for (uint8_t i = 0; i < snapshot.metricCount; i++) {
        output += "  ";
        output += snapshot.metrics[i].label;
        output += ": ";
        output += formatMetricValue(snapshot.metrics[i]);
        output += '\n';
    }
    return output;
}

String BoxDiagnostics::renderEmptySnapshots(DiagnosticFormat format) const
{
    if (format == DiagnosticFormat::Html) {
        return "<section class=\"diagnostic-snapshots\"><h2>Diagnostic snapshots</h2><p>No snapshots stored yet.</p></section>";
    }
    return "Diagnostic snapshots\n  No snapshots stored yet.\n";
}

String BoxDiagnostics::formatMetricValue(const DiagnosticMetric& metric) const
{
    String output = String(static_cast<unsigned long>(metric.value));
    if (metric.unit != nullptr && metric.unit[0] != '\0') {
        output += ' ';
        output += metric.unit;
        if (strcmp(metric.unit, "B") == 0) {
            output += " (";
            output += formatBytes(metric.value);
            output += ')';
        }
    }
    return output;
}

String BoxDiagnostics::formatBytes(uint32_t bytes) const
{
    char buffer[24] = {};
    if (bytes >= 1024UL * 1024UL) {
        snprintf(buffer, sizeof(buffer), "%.2f MiB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else if (bytes >= 1024UL) {
        snprintf(buffer, sizeof(buffer), "%.2f KiB", static_cast<double>(bytes) / 1024.0);
    } else {
        snprintf(buffer, sizeof(buffer), "%lu B", static_cast<unsigned long>(bytes));
    }
    return String(buffer);
}
