#pragma once

#include "qr_scanner.h"
#include "state_machine.h"

// UART/scanner events are translated into state-machine events in the main loop.
class BoxScanner {
public:
    explicit BoxScanner(BoxStateMachine& stateMachine) : machine(stateMachine) {}
    bool begin(Stream& serial);
    void onStateChanged(BoxState oldState, BoxState newState);
    void update(unsigned long now);

private:
    BoxStateMachine& machine;
    QrScanner scanner;
    Stream* uart = nullptr;
    bool ready = false;
    uint8_t activateBytes[QR_SCANNER_MAX_FRAME_LEN] = {};
    uint8_t deactivateBytes[QR_SCANNER_MAX_FRAME_LEN] = {};
    void sendEvent(BoxEventType type);
};
