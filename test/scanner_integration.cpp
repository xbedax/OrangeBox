#include <cassert>
#include <iostream>
#include <cstring>
#include "box_scanner.h"
#include "box_display.h"
#include "keyboard.h"
#include "pass_store.h"
#include "logger.h"
#include "timer.h"
#include "Wire.h"

BoxDisplay boxDisplay;
GpioHAL gpioHal;
CredStorage credStorage;
TimerManager timerManager;
Logger logger;
static uint8_t actionLine, infoLine, responseLine, progress, uiAction;
static uint8_t contactState = DOOR_CLOSED;
static unsigned openCalls;
static bool openSucceeds = true;
static BoxScanner* bridge;
void BoxDisplay::setOpenDoorList() {}
void BoxDisplay::writeStatusLine() {}
void BoxDisplay::writeInfoLine(uint8_t v) { infoLine = v; }
void BoxDisplay::writeActionLine(uint8_t v) { actionLine = v; }
void BoxDisplay::writeResponseLine(uint8_t v) { responseLine = v; }
void BoxDisplay::setPasswordLength(uint8_t) {}
void BoxDisplay::setVerifyCode(const char*) {}
void BoxDisplay::setProgressBar(uint8_t v) { progress = v; responseLine = RESPONSELINE_PROGRESS; }
uint8_t GpioHAL::readDoorState(uint8_t) { return contactState; }
uint8_t GpioHAL::openDoor(uint8_t door) { ++openCalls; return openSucceeds ? door : DOOR_UNKNOWN; }
uint8_t GpioHAL::ambientOn() { return 1; }
uint8_t GpioHAL::ambientOff() { return 0; }
void GpioHAL::getOpenDoors(bool* doors, uint8_t count) { memset(doors, 0, count); }
void Logger::logPrint(uint8_t, const char*, const char*, uint8_t) {}
void Logger::logPrint(uint8_t, const String&, const char*, uint8_t) {}

static void stateChanged(BoxState oldState, BoxState newState, unsigned long) { bridge->onStateChanged(oldState, newState); }
static void event(BoxStateMachine& machine, BoxEventType type, uint8_t door = 0) {
    BoxEventData e = {};
    e.eventType = type;
    e.data.doorData.doorNum = door;
    machine.processEvent(e);
}
static void pump(BoxScanner& scanner) { for (int i = 0; i < 4; ++i) scanner.update(testStubMillis); }
static void tick(BoxStateMachine& machine, BoxScanner& scanner, uint32_t now) {
    testStubMillis = now;
    machine.update(now);
    pump(scanner);
}
static void receive(Stream& uart, BoxScanner& scanner, const std::string& frame) {
    for (uint8_t c : frame) uart.input.push_back(c);
    pump(scanner);
}
static uint32_t add(CredType type, const char* value) {
    CacheRecord cred = {};
    cred.credType = type;
    strcpy(cred.name, "scan-test");
    if (type == CredType::PacketNumber) strcpy(cred.packetNumber, value);
    else strcpy(cred.pin, value);
    cred.remaining = 1;
    cred.doorNum = 0;
    const uint32_t id = credStorage.addCred(cred);
    assert(id);
    return id;
}

int main() {
    for (uint32_t start : {uint32_t(0), UINT32_MAX - 2000U}) {
        Preferences::clearAll();
        credStorage = CredStorage();
        assert(credStorage.begin());
        const uint32_t packetId = add(CredType::PacketNumber, "HZ1268956754M");
        const uint32_t passwordId = add(CredType::Password, "12345678");
        Stream uart;
        BoxStateMachine machine;
        BoxScanner scanner(machine);
        bridge = &scanner;
        assert(scanner.begin(uart));
        machine.setStateChangeCallback(stateChanged);
        machine.setUIUpdateCallback([](uint8_t v) { uiAction = v; });
        testStubMillis = start;
        machine.initialize();
        BoxKeyboard keyboard;
        keyboardStatus keys = {};
        Wire.pushKey(KEYBOARD_KEY_3);
        keyboard.handleKeyboard(&keys, &machine);
        assert(machine.getCurrentState() == BoxState::Scan);
        assert(actionLine == ACTIONLINE_SCAN && infoLine == INFOLINE_CANCEL);
        assert(progress == 100 && responseLine == RESPONSELINE_PROGRESS);
        assert(uiAction == UI_DISABLE_DOOR_CONTROLS);
        const std::vector<uint8_t> expectedCommand = {0x7e, 0x00, 0x08, 0x01, 0x00, 0x02, 0x01, 0xab, 0xcd};
        assert(uart.output == expectedCommand); // Binary command, not its hex spelling.
        tick(machine, scanner, start + QR_SCANNER_SCAN_TIMEOUT_MS / 2);
        assert(progress == 50 && machine.getCurrentState() == BoxState::Scan);
        tick(machine, scanner, start + QR_SCANNER_SCAN_TIMEOUT_MS);
        assert(machine.getCurrentState() == BoxState::Home);
        assert(uiAction == UI_ENABLE_DOOR_CONTROLS);
        unsigned before = openCalls;
        receive(uart, scanner, "HZ1268956754M\r\n");
        assert(openCalls == before); // Late frames outside Scan are ignored.

        // '*' cancels, '3' is not a restart while already scanning.
        event(machine, BoxEventType::KeyboardKey3);
        const size_t sent = uart.output.size();
        event(machine, BoxEventType::KeyboardKey3);
        assert(uart.output.size() == sent);
        Wire.pushKey(KEYBOARD_KEY_CANCEL);
        keyboard.handleKeyboard(&keys, &machine);
        assert(machine.getCurrentState() == BoxState::Home);
        // Discard a stale buffered frame before the next activation.
        for (char c : std::string("HZ1268956754M\r")) uart.input.push_back(c);
        event(machine, BoxEventType::KeyboardKey3);
        pump(scanner);
        assert(machine.getCurrentState() == BoxState::Scan && uart.input.empty());
        event(machine, BoxEventType::KeyboardCancel);

        // An invalid code returns Home immediately without a password penalty.
        event(machine, BoxEventType::KeyboardKey3);
        receive(uart, scanner, "INVALID12345\r");
        assert(machine.getCurrentState() == BoxState::Home && openCalls == before);
        assert(machine.getBadPasswordCount() == 0);
        event(machine, BoxEventType::KeyboardKey3);
        receive(uart, scanner, "12345678\r");
        assert(machine.getCurrentState() == BoxState::Home && openCalls == before); // Password is not a packet.

        // Binary acknowledgement is ignored; a fast text code can follow within 300 ms.
        event(machine, BoxEventType::KeyboardKey3);
        receive(uart, scanner, std::string("\x06\x00\r", 3));
        assert(machine.getCurrentState() == BoxState::Scan);
        receive(uart, scanner, "HZ126895OTHER\r\n"); // Same configured prefix.
        assert(machine.getCurrentState() == BoxState::Opening && openCalls == before + 1);
        CacheRecord record;
        assert(credStorage.getCred(packetId, record, CredType::PacketNumber)); // Not consumed before contact opens.
        event(machine, BoxEventType::DoorOpened, 1);
        assert(machine.getCurrentState() == BoxState::Opening); // Wrong door.
        event(machine, BoxEventType::DoorOpened, 0);
        assert(machine.getCurrentState() == BoxState::Open);
#ifdef QR_SCANNER_TEST_KEEP_CRED
        assert(credStorage.getCred(packetId, record, CredType::PacketNumber) && record.remaining == 1);
#else
        assert(!credStorage.getCred(packetId, record, CredType::PacketNumber));
#endif
        event(machine, BoxEventType::DoorClosed, 0);
        tick(machine, scanner, uint32_t(testStubMillis + AMBIENT_TIMEOUT));
        assert(machine.getCurrentState() == BoxState::Home);

        // Even in scanner test mode, password credentials are still consumed.
        event(machine, BoxEventType::KeyboardKey1);
        BoxEventData password = {};
        password.eventType = BoxEventType::KeyboardEnter;
        strcpy(password.data.keyboardData.password, "12345678");
        machine.processEvent(password);
        assert(machine.getCurrentState() == BoxState::Opening);
        event(machine, BoxEventType::DoorOpened);
        assert(!credStorage.getCred(passwordId, record, CredType::Password));
        event(machine, BoxEventType::DoorClosed);
        tick(machine, scanner, uint32_t(testStubMillis + AMBIENT_TIMEOUT));

        // Deadline is checked before accepting even a correctly framed code.
        const uint32_t nextPacketId = add(CredType::PacketNumber, "DR4449786543LE");
        event(machine, BoxEventType::KeyboardKey3);
        before = openCalls;
        testStubMillis = uint32_t(testStubMillis + QR_SCANNER_SCAN_TIMEOUT_MS);
        receive(uart, scanner, "DR4449786543LE\r");
        assert(machine.getCurrentState() == BoxState::Home && openCalls == before);

        // A frame without CR/LF is completed by idle time, just before the deadline.
        event(machine, BoxEventType::KeyboardKey3);
        tick(machine, scanner, uint32_t(testStubMillis + QR_SCANNER_SCAN_TIMEOUT_MS - QR_SCANNER_FRAME_IDLE_MS - 1));
        receive(uart, scanner, "DR4449786543LE");
        assert(machine.getCurrentState() == BoxState::Scan);
        tick(machine, scanner, uint32_t(testStubMillis + QR_SCANNER_FRAME_IDLE_MS));
        assert(machine.getCurrentState() == BoxState::Opening && openCalls == before + 1);
        tick(machine, scanner, uint32_t(testStubMillis + DOOR_OPENING_TIMEOUT));
        assert(machine.getCurrentState() == BoxState::Home);
        assert(credStorage.getCred(nextPacketId, record, CredType::PacketNumber) && record.remaining == 1);

        // Overflow and embedded NUL must not be accepted as a truncated credential.
        event(machine, BoxEventType::KeyboardKey3);
        tick(machine, scanner, uint32_t(testStubMillis + QR_SCANNER_RESPONSE_WINDOW_MS + 1));
        receive(uart, scanner, std::string(QR_SCANNER_MAX_FRAME_LEN + 1, 'A'));
        assert(machine.getCurrentState() == BoxState::Home);
        event(machine, BoxEventType::KeyboardKey3);
        tick(machine, scanner, uint32_t(testStubMillis + QR_SCANNER_RESPONSE_WINDOW_MS + 1));
        receive(uart, scanner, std::string("HZ126895\0BAD\r", 13));
        assert(machine.getCurrentState() == BoxState::Home);

        // A web open request cancels scanning; later scanner data cannot redirect it.
        event(machine, BoxEventType::KeyboardKey3);
        event(machine, BoxEventType::WebOpenBox, 1);
        receive(uart, scanner, "HZ1268956754M\r");
        assert(machine.getCurrentState() == BoxState::Opening && machine.getDoorToOpen() == 1);
        tick(machine, scanner, uint32_t(testStubMillis + DOOR_OPENING_TIMEOUT));
        assert(machine.getCurrentState() == BoxState::Home);
    }
    // Missing initialization fails to Home instead of leaving a stuck countdown.
    BoxStateMachine machine;
    BoxScanner uninitialized(machine);
    bridge = &uninitialized;
    machine.setStateChangeCallback(stateChanged);
    machine.initialize();
    event(machine, BoxEventType::KeyboardKey3);
    assert(machine.getCurrentState() == BoxState::Home);
    std::cout << "PASS: scanner integration (keyboard, UART, countdown, rollover, credentials and consumption)\n";
}
