#include "box_scanner.h"
#include "logger.h"
#include "qr_payload.h"
#include "timeout.h"
#include <cstring>

namespace {
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
    if (!QrPayload::parse(QR_SCANNER_ACTIVATE_COMMAND, activateBytes, sizeof(activateBytes), activateLength)
        || !QrPayload::parse(QR_SCANNER_DEACTIVATE_COMMAND, deactivateBytes, sizeof(deactivateBytes), deactivateLength))
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
        logger.logPrint(A_SCANNER_BUFFER_OVERFLOW, LOGAREA_SYSTEM, SEVERITY_ERROR);
        sendEvent(BoxEventType::ScannerInvalidCode);
    } else if (scanned.type == QrScannerEventType::CodeReceived
        // A quick scan may arrive inside the activation response window.
        // Ignore short acknowledgements and binary command responses there.
        || (scanned.type == QrScannerEventType::ActivationResponse
            && scanned.length >= PACKET_NUMBER_MIN && isText(scanned))) {
        if (scanned.length < PACKET_NUMBER_MIN || scanned.length > PACKET_SCAN_MAX || !isText(scanned)) {
            sendEvent(BoxEventType::ScannerInvalidCode);
            return;
        }
        BoxEventData event = {};
        event.eventType = BoxEventType::ScannerCodeReceived;
        memcpy(event.data.scannerData.cred, scanned.data, scanned.length);
        event.data.scannerData.cred[scanned.length] = '\0';
        machine.processEvent(event);
    }
}
