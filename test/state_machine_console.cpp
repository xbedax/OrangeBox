#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "Arduino.h"
#include "Wire.h"
#include "box_display.h"
#include "config.h"
#include "gpio_hal.h"
#include "keyboard.h"
#include "pass_store.h"
#include "state_machine.h"
#include "timer.h"

const char* fversion = _VERSION_;

BoxDisplay boxDisplay;
GpioHAL gpioHal;
PinStorage pinStorage;
TimerManager timerManager;
BoxKeyboard boxKeyboard;
BoxStateMachine boxStateMachine;
keyboardStatus keyboardState = {};

DoorMapping initialDoorMappings[] = INITIAL_DOOR_MAPPING;

static const char* stateName(BoxState state)
{
    switch (state) {
        case BoxState::Home: return "Home";
        case BoxState::Password: return "Password";
        case BoxState::Presence: return "Presence";
        case BoxState::Opening: return "Opening";
        case BoxState::Open: return "Open";
        case BoxState::Closed: return "Closed";
        case BoxState::External: return "External";
        case BoxState::BadPass: return "BadPass";
        default: return "?";
    }
}

static void onStateChanged(BoxState oldState, BoxState newState, unsigned long)
{
    std::cout << "\n[STATE] " << stateName(oldState) << " -> " << stateName(newState) << "\n";

    if (newState == BoxState::Password) {
        keyboardState.keyboardMode = KEYBOARD_MODE_PASSWORD;
        keyboardState.currentPasswordLen = 0;
        keyboardState.passwordComplete = false;
        keyboardState.cancelPressed = false;
        boxKeyboard.clearPassword(&keyboardState);
    } else {
        keyboardState.keyboardMode = KEYBOARD_MODE_COMMAND;
    }
}

static void handleTimerAction(uint8_t action, const char* arg)
{
    if (action == LOCK_DEACTIVATE) {
        uint8_t doorNum = static_cast<uint8_t>(std::atoi(arg ? arg : "0"));
        gpioHal.lockDeactivate(doorNum);
    }
}

static void setSimulatedDoorState(uint8_t doorNum, uint8_t doorState)
{
    bool isOpen = doorState == DOOR_OPEN;
    for (size_t i = 0; i < sizeof(initialDoorMappings) / sizeof(initialDoorMappings[0]); i++) {
        if (initialDoorMappings[i].logDoorNum == doorNum
            && initialDoorMappings[i].statePin != INVALID_PIN) {
            digitalWrite(initialDoorMappings[i].statePin, isOpen ? HIGH : LOW);
        }
    }
    gpioHal.setDoorLastState(doorNum, isOpen);
}

static void runStep(unsigned long advanceMs = 100)
{
    advanceMillis(advanceMs);
    timerManager.update(millis());
    boxKeyboard.handleKeyboard(&keyboardState, &boxStateMachine);
    Serial.print ("Mil: " + String(millis()) + ", State: " + String(static_cast<int>(boxStateMachine.getCurrentState())) + ", Door: " + String(boxStateMachine.getDoorToOpen()) + ", Keyb: " + String(static_cast<int>(keyboardState.keyboardMode)) + "\n");
    boxStateMachine.update(millis());

}

static void sendDoorEvent(BoxEventType eventType)
{
    BoxEventData event = {};
    event.eventType = eventType;
    event.data.doorData.doorNum = boxStateMachine.getDoorToOpen();
    if (event.data.doorData.doorNum == 0) {
        event.data.doorData.doorNum = 1;
    }
    event.data.doorData.doorState = eventType == BoxEventType::DoorOpened ? DOOR_OPEN : DOOR_CLOSED;
    setSimulatedDoorState(event.data.doorData.doorNum, event.data.doorData.doorState);
    boxStateMachine.processEvent(event);
}

static bool feedChar(char ch)
{
    if (ch == '\r' || ch == '\n') {
        runStep(100);
        return true;
    }

    if (ch == 'q' || ch == 'Q') {
        return false;
    }

    if (ch == '.') {
        runStep(1000);
        return true;
    }

    if (ch == '>') {
        runStep(10000);
        return true;
    }

    if (ch == 'o' || ch == 'O') {
        sendDoorEvent(BoxEventType::DoorOpened);
        runStep();
        return true;
    }

    if (ch == 'c' || ch == 'C') {
        sendDoorEvent(BoxEventType::DoorClosed);
        runStep();
        return true;
    }

    if (ch >= '0' && ch <= '9') {
        Wire.pushKey(static_cast<uint8_t>(ch - '0'));
        runStep();
        return true;
    }

    if (ch == '#') {
        Wire.pushKey(KEYBOARD_KEY_ENTER);
        runStep();
        return true;
    }

    if (ch == '*') {
        Wire.pushKey(KEYBOARD_KEY_CANCEL);
        runStep();
        return true;
    }

    return true;
}

int main()
{
    Serial.begin(9600);
    timerManager.initializeTimerManager(handleTimerAction);
    gpioHal.initializeGpioHAL(
        &timerManager,
        initialDoorMappings,
        AMBIENT_PIN,
        sizeof(initialDoorMappings) / sizeof(initialDoorMappings[0]));
    boxDisplay.displayInit(&gpioHal);

    keyboardState.keyboardMode = KEYBOARD_MODE_COMMAND;
    keyboardState.currentPasswordLen = 0;
    keyboardState.passwordComplete = false;
    keyboardState.cancelPressed = false;
    boxKeyboard.clearPassword(&keyboardState);
    boxKeyboard.keyboardInit();

    pinStorage.begin();
    boxStateMachine.initialize();
    boxStateMachine.setStateChangeCallback(onStateChanged);

    std::cout
        << "\nState machine console\n"
        << "Keys: 1=password mode, 2=presence, digits=PIN, #=enter, *=cancel/backspace\n"
        << "Fake valid PINs: 1234, 123456. Example: 11234# opens door 1.\n"
        << "Extra: o=door opened, c=door closed, .=+1s, >=+10s, q=quit\n\n";

    char ch;
    while (std::cin.get(ch)) {
        if (!feedChar(ch)) {
            break;
        }
    }

    return 0;
}
