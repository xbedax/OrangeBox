#include <cassert>
#include <iostream>
#include "state_machine.h"
#include "box_display.h"
#include "gpio_hal.h"
#include "pass_store.h"
#include "logger.h"
#include "timer.h"

BoxDisplay boxDisplay;
GpioHAL gpioHal;
CredStorage credStorage;
TimerManager timerManager;
Logger logger;
uint8_t CredStorage::verifyCred(const char*, CredType) { return DOOR_UNKNOWN; }
bool CredStorage::usedCred(uint8_t) { return true; }
static uint8_t progress;
static bool ambient;
static bool openingSucceeds = true;
static uint8_t lastUIAction;
static unsigned uiActionCount;
static void recordUIAction(uint8_t action) { lastUIAction = action; ++uiActionCount; }
void BoxDisplay::setOpenDoorList() {}
void BoxDisplay::writeStatusLine() {}
void BoxDisplay::writeInfoLine(uint8_t) {}
void BoxDisplay::writeActionLine(uint8_t) {}
void BoxDisplay::writeResponseLine(uint8_t) {}
void BoxDisplay::setPasswordLength(uint8_t) {}
void BoxDisplay::setVerifyCode(const char*) {}
void BoxDisplay::setProgressBar(uint8_t value) { progress = value; }
uint8_t GpioHAL::readDoorState(uint8_t) { return DOOR_CLOSED; }
uint8_t GpioHAL::openDoor(uint8_t door) { return openingSucceeds ? door : DOOR_UNKNOWN; }
uint8_t GpioHAL::ambientOn() { ambient = true; return 1; }
uint8_t GpioHAL::ambientOff() { ambient = false; return 0; }
void GpioHAL::getOpenDoors(bool* doors, uint8_t count) { memset(doors, 0, count); }
void Logger::logPrint(uint8_t, const char*, const char*, uint8_t) {}
void Logger::logPrint(uint8_t, const String&, const char*, uint8_t) {}

static void event(BoxStateMachine& machine, BoxEventType type, uint8_t door = 1)
{
    BoxEventData data = {};
    data.eventType = type;
    data.data.doorData.doorNum = door;
    machine.processEvent(data);
}
static void tick(BoxStateMachine& machine, uint32_t now)
{
    testStubMillis = now;
    machine.update(now);
}
int main()
{
    for (uint32_t start : {uint32_t(0), UINT32_MAX - 999U}) {
        testStubMillis = start;
        BoxStateMachine machine;
        machine.setUIUpdateCallback(recordUIAction);
        machine.initialize();
        assert(lastUIAction == UI_ENABLE_DOOR_CONTROLS);
        assert(!machine.isPresenceCodeValid());
        event(machine, BoxEventType::KeyboardKey1);
        assert(lastUIAction == UI_DISABLE_DOOR_CONTROLS);
        const uint32_t passwordDuration = PASS_ENTRY_TIMEOUT * 1000UL;
        tick(machine, start + passwordDuration / 2);
        assert(machine.getCurrentState() == BoxState::Password);
        assert(progress == 50);
        tick(machine, start + passwordDuration - 1);
        assert(machine.getCurrentState() == BoxState::Password);
        tick(machine, start + passwordDuration);
        assert(machine.getCurrentState() == BoxState::Home);
        assert(lastUIAction == UI_ENABLE_DOOR_CONTROLS);

        testStubMillis = start;
        machine.initialize();
        event(machine, BoxEventType::KeyboardKey2);
        const uint32_t presenceDuration = PRESENCE_CODE_VALIDITY * 1000UL;
        tick(machine, start + presenceDuration / 2);
        assert(machine.isPresenceCodeValid() && progress == 50);
        testStubMillis = uint32_t(start + presenceDuration);
        assert(!machine.isPresenceCodeValid()); // Web request before the next update.
        tick(machine, start + presenceDuration);
        assert(machine.getCurrentState() == BoxState::Home);
        tick(machine, start); // Expired code must not revive on a later wrap.
        assert(!machine.isPresenceCodeValid());

        machine.initialize();
        event(machine, BoxEventType::KeyboardKey1);
        const unsigned beforeBadPassword = uiActionCount;
        BoxEventData badPin = {};
        badPin.eventType = BoxEventType::KeyboardEnter;
        strcpy(badPin.data.keyboardData.password, "9999");
        machine.processEvent(badPin);
        const uint32_t penalty = PASS_ERR_DELAY * 1000UL;
        assert(machine.getCurrentState() == BoxState::BadPass);
        tick(machine, start + penalty / 2);
        assert(progress == 50);
        event(machine, BoxEventType::KeyboardCancel);
        event(machine, BoxEventType::KeyboardKey1);
        assert(machine.getCurrentState() == BoxState::BadPass);
        tick(machine, start + penalty);
        assert(machine.getCurrentState() == BoxState::Password);
        // Cancel re-enables Home, Key1 disables again; BadPass -> Password adds nothing.
        assert(uiActionCount == beforeBadPassword + 2);

        testStubMillis = start;
        machine.initialize();
        event(machine, BoxEventType::WebOpenBox);
        tick(machine, start + DOOR_OPENING_TIMEOUT - 1);
        assert(machine.getCurrentState() == BoxState::Opening && ambient);
        tick(machine, start + DOOR_OPENING_TIMEOUT);
        assert(machine.getCurrentState() == BoxState::Home && !ambient);

        // The explicit timeout event must perform the same Home cleanup.
        testStubMillis = start;
        machine.initialize();
        event(machine, BoxEventType::WebOpenBox);
        assert(ambient);
        event(machine, BoxEventType::DoorOpenTimeout);
        assert(machine.getCurrentState() == BoxState::Home && !ambient);

        testStubMillis = start;
        machine.initialize();
        event(machine, BoxEventType::KeyboardKey1);
        const unsigned beforeOpening = uiActionCount;
        event(machine, BoxEventType::WebOpenBox);
        assert(uiActionCount == beforeOpening); // Already disabled while entering the password.
        event(machine, BoxEventType::DoorOpened);
        event(machine, BoxEventType::DoorClosed);
        assert(ambient);
        tick(machine, start + AMBIENT_TIMEOUT - 1);
        assert(machine.getCurrentState() == BoxState::Closed && ambient);
        tick(machine, start + AMBIENT_TIMEOUT);
        assert(machine.getCurrentState() == BoxState::Home && !ambient);
    }
    // Web request from Home: the first loop update must keep the light on until
    // the door opens, then closing starts the normal delayed return to Home.
    testStubMillis = 100;
    BoxStateMachine webMachine;
    webMachine.setUIUpdateCallback(recordUIAction);
    webMachine.initialize();
    const unsigned beforeWebOpen = uiActionCount;
    event(webMachine, BoxEventType::WebOpenBox, 0);
    tick(webMachine, 101);
    assert(webMachine.getCurrentState() == BoxState::Opening && ambient);
    assert(uiActionCount == beforeWebOpen + 1 && lastUIAction == UI_DISABLE_DOOR_CONTROLS);
    event(webMachine, BoxEventType::DoorOpened, 0);
    tick(webMachine, 100 + DOOR_OPENING_TIMEOUT + 10);
    assert(webMachine.getCurrentState() == BoxState::Open && ambient);
    event(webMachine, BoxEventType::DoorClosed, 0);
    const uint32_t closedAt = testStubMillis;
    tick(webMachine, closedAt + AMBIENT_TIMEOUT - 1);
    assert(webMachine.getCurrentState() == BoxState::Closed && ambient);
    tick(webMachine, closedAt + AMBIENT_TIMEOUT);
    assert(webMachine.getCurrentState() == BoxState::Home && !ambient);
    assert(uiActionCount == beforeWebOpen + 2 && lastUIAction == UI_ENABLE_DOOR_CONTROLS);
    testStubMillis = 0;
    BoxStateMachine machine;
        machine.setUIUpdateCallback(nullptr);
    machine.initialize();
    openingSucceeds = false;
    event(machine, BoxEventType::WebOpenBox);
    tick(machine, 0);
    assert(machine.getCurrentState() == BoxState::Home);
    std::cout << "State machine rollover tests passed\n";
}
