#include "box_scanner.h"
#include "timeout.h"
#include <cstring>

namespace {
int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Configuration commands contain hex bytes (optionally prefixed with "hex:").
bool decodeCommand(const char* text, uint8_t* output, size_t capacity, size_t& length) {
    length = 0;
    while (*text == ' ' || *text == '\t') ++text;
    if (strncmp(text, "hex:", 4) == 0) text += 4;
    int high = -1;
    for (; *text; ++text) {
        if (*text == ' ' || *text == '\t' || *text == ',' || *text == ':' || *text == '-') continue;
        const int digit = hexDigit(*text);
        if (digit < 0) return false;
        if (high < 0) high = digit;
        else {
            if (length == capacity) return false;
            output[length++] = static_cast<uint8_t>((high << 4) | digit);
            high = -1;
        }
    }
    return high < 0;
}

bool isText(const QrScannerEvent& event) {
    if (!event.data || event.length == 0) return false;
    for (size_t i = 0; i < event.length; ++i)
        if (event.data[i] < 33 || event.data[i] > 126) return false;
    return true;
}
}

bool BoxScanner::begin(Stream& serial) {
    ready = false;
    uart = &serial;
    size_t activateLength, deactivateLength;
    if (!decodeCommand(QR_SCANNER_ACTIVATE_COMMAND, activateBytes, sizeof(activateBytes), activateLength)
        || !decodeCommand(QR_SCANNER_DEACTIVATE_COMMAND, deactivateBytes, sizeof(deactivateBytes), deactivateLength))
        return false;
    QrScannerConfig config;
    config.activateCommand = {activateBytes, activateLength};
    config.deactivateCommand = {deactivateBytes, deactivateLength};
    config.switchPin = QR_SCANNER_SWITCH_PIN;
    config.switchActiveHigh = QR_SCANNER_SWITCH_ACTIVE_HIGH != 0;
    config.activationResponseWindowMs = QR_SCANNER_RESPONSE_WINDOW_MS;
    config.commandResponseWindowMs = QR_SCANNER_COMMAND_RESPONSE_WINDOW_MS;
    config.frameIdleMs = QR_SCANNER_FRAME_IDLE_MS;
    config.defaultScanTimeoutMs = QR_SCANNER_SCAN_TIMEOUT_MS;
    const bool hasSwitch = config.switchPin != QR_SCANNER_PIN_UNUSED;
    config.activationMode = activateLength
        ? (hasSwitch ? QrScannerActivationMode::CommandAndSwitch : QrScannerActivationMode::Command)
        : (hasSwitch ? QrScannerActivationMode::SwitchPin : QrScannerActivationMode::Manual);
    scanner.begin(serial, config);
    ready = true;
    return true;
}

void BoxScanner::sendEvent(BoxEventType type) {
    BoxEventData event = {};
    event.eventType = type;
    machine.processEvent(event);
}

void BoxScanner::onStateChanged(BoxState oldState, BoxState newState) {
    if (oldState == BoxState::Scan && ready && scanner.isActive()) scanner.deactivate();
    if (newState != BoxState::Scan) return;
    if (!ready) {
        sendEvent(BoxEventType::ScannerFailed);
        return;
    }
    // A frame left over from an earlier attempt must not authenticate a new scan.
    while (uart->available() > 0) if (uart->read() < 0) break;
    if (!scanner.startScan(QR_SCANNER_SCAN_TIMEOUT_MS)) sendEvent(BoxEventType::ScannerFailed);
}

void BoxScanner::update(unsigned long now) {
    if (!ready) return;
    if (machine.getCurrentState() == BoxState::Scan
        && timeoutElapsed(machine.getContext().scanStartedAt, QR_SCANNER_SCAN_TIMEOUT_MS, now)) {
        sendEvent(BoxEventType::ScanTimeout);
        return;
    }
    const QrScannerEvent scanned = scanner.update(now);
    if (machine.getCurrentState() != BoxState::Scan) return;
    if (scanned.type == QrScannerEventType::ScanTimeout) {
        sendEvent(BoxEventType::ScanTimeout);
    } else if (scanned.type == QrScannerEventType::RxOverflow) {
        sendEvent(BoxEventType::ScannerFailed);
    } else if (scanned.type == QrScannerEventType::CodeReceived
        // A quick scan may arrive inside the activation response window.
        // Ignore short acknowledgements and binary command responses there.
        || (scanned.type == QrScannerEventType::ActivationResponse
            && scanned.length >= PACKET_NUMBER_MIN && isText(scanned))) {
        if (scanned.length < PACKET_NUMBER_MIN || scanned.length > PACKET_NUMBER_MAX || !isText(scanned)) {
            sendEvent(BoxEventType::ScannerFailed);
            return;
        }
        BoxEventData event = {};
        event.eventType = BoxEventType::ScannerCodeReceived;
        memcpy(event.data.scannerData.cred, scanned.data, scanned.length);
        event.data.scannerData.cred[scanned.length] = '\0';
        machine.processEvent(event);
    }
}
