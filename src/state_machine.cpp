#include "state_machine.h"
#include "box_display.h"
#include "gpio_hal.h"
#include "pass_store.h"
#include "timer.h"
#include "config.h"
#include <cstring>

// Forward declarations of external objects (to be linked from main.cpp)
extern BoxDisplay boxDisplay;
extern GpioHAL gpioHal;
extern PinStorage pinStorage;
extern TimerManager timerManager;

// Constants from config
//#define DOOR_OPENING_TIMEOUT 5000           // ms - from main.cpp
//#define AMBIENT_TIMEOUT 10000               // ms - after door closes

BoxStateMachine::BoxStateMachine() : onStateChange(nullptr) {
    memset(&context, 0, sizeof(BoxStateContext));
    context.state = BoxState::Home;
    context.doorToOpen = 0;
    context.badPasswordCount = 0;
}

void BoxStateMachine::initialize() {
    unsigned long currentMillis = millis();
    context.state = BoxState::Home;
    context.doorToOpen = 0;
    context.badPasswordCount = 0;
    context.doorOpenTimeout = 0;
    context.passwordEntryTimeout = 0;
    context.badPasswordDelayFinish = 0;
    context.presenceCodeExpiration = 0;
    context.ambientOffTimeout = 0;
    context.displayActionMillis = 0;
    context.currentPasswordEntryMillis = 0;
    context.currentBadPasswordMillis = 0;

    memset(context.presenceCode, 0, sizeof(context.presenceCode));
    onEnterHome(currentMillis);
    scheduleNextDisplayRefresh(currentMillis);
}

BoxState BoxStateMachine::getCurrentState() const {
    return context.state;
}

const BoxStateContext& BoxStateMachine::getContext() const {
    return context;
}

void BoxStateMachine::processEvent(const BoxEventData& event) {
    unsigned long currentMillis = millis();

    switch (event.eventType) {
        case BoxEventType::KeyboardKey1:
        case BoxEventType::KeyboardKey2:
        case BoxEventType::KeyboardEnter:
        case BoxEventType::KeyboardCancel:
            handleKeyboardEvent(event, currentMillis);
            break;
            
        case BoxEventType::WebOpenBox:
            handleWebEvent(event, currentMillis);
            break;
            
        case BoxEventType::DoorOpened:
        case BoxEventType::DoorClosed:
            handleSensorEvent(event, currentMillis);
            break;
            
        case BoxEventType::PasswordTimeout:
        case BoxEventType::PresenceTimeout:
        case BoxEventType::AmbientTimeout:
        case BoxEventType::DoorOpenTimeout:
            handleTimerEvent(event, currentMillis);
            break;
            
        default:
            break;
    }
} // processEvent

void BoxStateMachine::update(unsigned long currentMillis) {
    // Time-based state transitions and periodic updates
    if (isDisplayRefreshDue(currentMillis)) {
        if (refreshPeriodicDisplay(currentMillis)) {
            scheduleNextDisplayRefresh(currentMillis);
        } else {
            context.displayActionMillis = 0;
        }
    }

    switch (context.state) {
        case BoxState::Home:
            break;

        case BoxState::Password:
            if (context.passwordEntryTimeout < currentMillis && context.passwordEntryTimeout != 0) {
                transitionTo(BoxState::Home, currentMillis);
                break;
            }
            break;

        case BoxState::BadPass:
            // Check if bad-password penalty expired
            if (context.badPasswordDelayFinish < currentMillis && context.badPasswordDelayFinish != 0) {
                if (context.state == BoxState::BadPass) {
                    transitionTo(BoxState::Password, currentMillis);
                }
            }
            break;
            
        case BoxState::Presence:
            // Check if presence code expired
            if (context.presenceCodeExpiration < currentMillis && context.presenceCodeExpiration != 0) {
                transitionTo(BoxState::Home, currentMillis);
            }
            break;
            
        case BoxState::Opening:
            if (gpioHal.readDoorState(context.doorToOpen) == DOOR_OPEN) {
                transitionTo(BoxState::Open, currentMillis);
                break;
            }

            // Check if door opening timeout expired
            if (context.doorOpenTimeout < currentMillis && context.doorOpenTimeout != 0) {
                boxDisplay.logPrint("Error - door " + String(context.doorToOpen) + " not opened");
                transitionTo(BoxState::Home, currentMillis);
            }
            break;
            
        case BoxState::Closed:
            // Check if ambient light should be turned off
            if (context.ambientOffTimeout < currentMillis && context.ambientOffTimeout != 0) {
                transitionTo(BoxState::Home, currentMillis);
            }
            break;
            
        default:
            break;
    }
}

uint8_t BoxStateMachine::getDoorToOpen() const {
    return context.doorToOpen;
}

const char* BoxStateMachine::getPresenceCode() const {
    return context.presenceCode;
}

unsigned long BoxStateMachine::getPresenceCodeExpiration() const {
    return context.presenceCodeExpiration;
}

uint8_t BoxStateMachine::getBadPasswordCount() const {
    return context.badPasswordCount;
}

unsigned long BoxStateMachine::getBadPasswordDelayFinish() const {
    return context.badPasswordDelayFinish;
}

unsigned long BoxStateMachine::getAmbientOffTimeout() const {
    return context.ambientOffTimeout;
}

void BoxStateMachine::setStateChangeCallback(StateChangeCallback callback) {
    onStateChange = callback;
}

bool BoxStateMachine::isDisplayRefreshDue(unsigned long currentMillis) const {
    return context.displayActionMillis != 0
        && static_cast<int32_t>(currentMillis - context.displayActionMillis) >= 0;
}

void BoxStateMachine::scheduleNextDisplayRefresh(unsigned long currentMillis) {
    context.displayActionMillis = currentMillis + ACTIONLINE_REFRESH_INTERVAL;
}

bool BoxStateMachine::refreshPeriodicDisplay(unsigned long currentMillis) {
    boxDisplay.setOpenDoorList();
    boxDisplay.writeStatusLine();
    switch (context.state) {
        case BoxState::Home:
            boxDisplay.writeActionLine(ACTIONLINE_HOME);
            return true;

        case BoxState::Password:
            if (context.currentPasswordEntryMillis > 0) {
                unsigned long remaining = 0;
                if (context.passwordEntryTimeout > currentMillis) {
                    remaining = context.passwordEntryTimeout - currentMillis;
                }
                uint8_t progress = (uint8_t)((remaining * 100UL) / context.currentPasswordEntryMillis);
                boxDisplay.setProgressBar(progress);
            }
            return true;

        case BoxState::BadPass:
            boxDisplay.writeActionLine(ACTIONLINE_BADPASS);
            if (context.currentBadPasswordMillis > 0) {
                unsigned long remaining = 0;
                if (context.badPasswordDelayFinish > currentMillis) {
                    remaining = context.badPasswordDelayFinish - currentMillis;
                }
                uint8_t progress = (uint8_t)((remaining * 100UL) / context.currentBadPasswordMillis);
                boxDisplay.setProgressBar(progress);
            }
            return true;

        case BoxState::Presence:
            {
                unsigned long remaining = 0;
                if (context.presenceCodeExpiration > currentMillis) {
                    remaining = context.presenceCodeExpiration - currentMillis;
                }
                uint8_t progress = (uint8_t)((remaining * 100UL) / (PRESENCE_CODE_VALIDITY * 1000UL));
                boxDisplay.setProgressBar(progress);
            }
            return true;

        default:
            return false;
    }
}

void BoxStateMachine::transitionTo(BoxState newState, unsigned long currentMillis) {
    if (newState == context.state) {
        return; // No transition needed
    }
    
    // Exit current state
    switch (context.state) {
        case BoxState::Home:
            onExitHome();
            break;
        case BoxState::Password:
            onExitPassword();
            break;
        case BoxState::Presence:
            onExitPresence();
            break;
        case BoxState::Opening:
            onExitOpening();
            break;
        case BoxState::Open:
            onExitOpen();
            break;
        case BoxState::Closed:
            onExitClosed();
            break;
        case BoxState::External:
            onExitExternal();
            break;
        case BoxState::BadPass:
            onExitBadPass();
            break;
    }
    
    BoxState oldState = context.state;
    context.state = newState;
    
    // Enter new state
    switch (newState) {
        case BoxState::Home:
            onEnterHome(currentMillis);
            break;
        case BoxState::Password:
            onEnterPassword(currentMillis);
            break;
        case BoxState::Presence:
            onEnterPresence(currentMillis);
            break;
        case BoxState::Opening:
            onEnterOpening(currentMillis);
            break;
        case BoxState::Open:
            onEnterOpen(currentMillis);
            break;
        case BoxState::Closed:
            onEnterClosed(currentMillis);
            break;
        case BoxState::External:
            onEnterExternal(currentMillis);
            break;
        case BoxState::BadPass:
            onEnterBadPass(currentMillis);
            break;
    }

    scheduleNextDisplayRefresh(currentMillis);
    
    // Notify about state change
    if (onStateChange) {
        onStateChange(oldState, newState, currentMillis);
    }
}

void BoxStateMachine::handleKeyboardEvent(const BoxEventData& event, unsigned long currentMillis) {
    
    Serial.print("->>>>Keyboard event in state: " + String(static_cast<int>(context.state)) + ", event: " + String(static_cast<int>(event.eventType))); //>>>
    switch (context.state) {
        case BoxState::Home:
            if (event.eventType == BoxEventType::KeyboardKey1) {
                // Switch to password mode unless a bad-password penalty timeout is still active
                if (context.badPasswordDelayFinish > currentMillis) {
                    transitionTo(BoxState::BadPass, currentMillis);
                } else {
                    transitionTo(BoxState::Password, currentMillis);
                }
            } else if (event.eventType == BoxEventType::KeyboardKey2) {
                // Switch to presence mode
                transitionTo(BoxState::Presence, currentMillis);
            }
            break;
            
        case BoxState::Password:
            if (event.eventType == BoxEventType::KeyboardEnter) {
                // Validate password
                context.doorToOpen = pinStorage.usePin(event.data.keyboardData.password);
                if (context.doorToOpen > 0) {
                    // Valid PIN - transition to opening
                    context.badPasswordCount = 0;
                    transitionTo(BoxState::Opening, currentMillis);
                } else {
                    // Invalid PIN - transition to bad password state
                    context.badPasswordCount++;
                    transitionTo(BoxState::BadPass, currentMillis);
                }
                break;
            }
            // Fall through to share KeyboardCancel handling with BadPass and Presence.
            
        case BoxState::BadPass:
        case BoxState::Presence:
            if (event.eventType == BoxEventType::KeyboardCancel) {
                // Return to home
                transitionTo(BoxState::Home, currentMillis);
            }
            
            break;
            
        default:
            break;
    }
}

void BoxStateMachine::handleWebEvent(const BoxEventData& event, unsigned long currentMillis) {
    if (event.eventType == BoxEventType::WebOpenBox) {
        // Web request to open door
        context.doorToOpen = event.data.doorData.doorNum;
        transitionTo(BoxState::Opening, currentMillis);
    }
}

void BoxStateMachine::handleSensorEvent(const BoxEventData& event, unsigned long currentMillis) {
    boxDisplay.setOpenDoorList();
    Serial.print("->>>>Sensor event in state: " + String(static_cast<int>(context.state)) + ", event: " + String(static_cast<int>(event.eventType)) + ", door: " + String(event.data.doorData.doorNum)); //>>>.

    switch (context.state) {
        case BoxState::Opening:
            if (event.eventType == BoxEventType::DoorOpened) {
                if (event.data.doorData.doorNum == context.doorToOpen) {
                    transitionTo(BoxState::Open, currentMillis);
                }
            }
            break;
            
        case BoxState::Open:
            if (event.eventType == BoxEventType::DoorClosed) {
                if (event.data.doorData.doorNum == context.doorToOpen) {
                    transitionTo(BoxState::Closed, currentMillis);
                }
            }
            break;
            
        default:
            break;
    }
}

void BoxStateMachine::handleTimerEvent(const BoxEventData& event, unsigned long currentMillis) {
    switch (event.eventType) {
        case BoxEventType::PasswordTimeout:
            if (context.state == BoxState::Password) {
                transitionTo(BoxState::Home, currentMillis);
            }
            break;
            
        case BoxEventType::PresenceTimeout:
            if (context.state == BoxState::Presence) {
                transitionTo(BoxState::Home, currentMillis);
            }
            break;
            
        case BoxEventType::AmbientTimeout:
            if (context.state == BoxState::Closed) {
                transitionTo(BoxState::Home, currentMillis);
            }
            break;
            
        case BoxEventType::DoorOpenTimeout:
            if (context.state == BoxState::Opening) {
                transitionTo(BoxState::Home, currentMillis);
            }
            break;
            
        default:
            break;
    }
}

// State entry/exit actions

void BoxStateMachine::onEnterHome(unsigned long currentMillis) {
    enableExternal();
    boxDisplay.writeInfoLine(INFOLINE_HOME);            // INFOLINE_HOME
    boxDisplay.writeActionLine(ACTIONLINE_HOME);         // ACTIONLINE_HOME
    boxDisplay.writeResponseLine(RESPONSELINE_HOME);       // RESPONSELINE_HOME
    boxDisplay.writeStatusLine();
}

void BoxStateMachine::onExitHome() {
    // Nothing specific
}

void BoxStateMachine::onEnterPassword(unsigned long currentMillis) {
    disableExternal();
    boxDisplay.writeInfoLine(INFOLINE_PASS);            // INFOLINE_PASS
    boxDisplay.setPasswordLength(0);
    context.badPasswordDelayFinish = 0;
    context.currentBadPasswordMillis = 0;
    context.currentPasswordEntryMillis = (unsigned long)PASS_ENTRY_TIMEOUT * 1000UL;
    context.passwordEntryTimeout = currentMillis + context.currentPasswordEntryMillis;
    boxDisplay.setProgressBar(100);
    
}

void BoxStateMachine::onExitPassword() {
    context.passwordEntryTimeout = 0;
    context.currentPasswordEntryMillis = 0;
}

void BoxStateMachine::onEnterPresence(unsigned long currentMillis) {
    generatePresenceCode();
    context.presenceCodeExpiration = currentMillis + (PRESENCE_CODE_VALIDITY * 1000);
    boxDisplay.writeInfoLine(INFOLINE_CANCEL);            // INFOLINE_CANCEL
    boxDisplay.setVerifyCode(context.presenceCode);
    boxDisplay.setProgressBar(100);
}

void BoxStateMachine::onExitPresence() {
    // Nothing specific
}

void BoxStateMachine::onEnterOpening(unsigned long currentMillis) {
    disableExternal();
    boxDisplay.writeResponseLine(RESPONSELINE_OPENING);
    if (gpioHal.openDoor(context.doorToOpen) == context.doorToOpen) {
        context.doorOpenTimeout = currentMillis + DOOR_OPENING_TIMEOUT;
        gpioHal.ambientOn();
    } else {
        boxDisplay.logPrint("Error - door " + String(context.doorToOpen) + " cannot be opened");
        context.doorOpenTimeout = currentMillis;
    }
}

void BoxStateMachine::onExitOpening() {
    context.doorOpenTimeout = 0;
}

void BoxStateMachine::onEnterOpen(unsigned long currentMillis) {
    boxDisplay.writeActionLine(ACTIONLINE_CLOSE);         // ACTIONLINE_CLOSE
    boxDisplay.writeInfoLine(INFOLINE_EMPTY);            // INFOLINE_EMPTY
    boxDisplay.writeResponseLine(RESPONSELINE_EMPTY);        // RESPONSELINE_EMPTY
    boxDisplay.writeStatusLine();
    // cameraStart() - call from main.cpp
    
}

void BoxStateMachine::onExitOpen() {
    // Nothing specific
}

void BoxStateMachine::onEnterClosed(unsigned long currentMillis) {
    boxDisplay.writeActionLine(ACTIONLINE_CLOSETHX);         // ACTIONLINE_CLOSETHX
    boxDisplay.writeInfoLine(INFOLINE_EMPTY);            // INFOLINE_EMPTY
    boxDisplay.writeResponseLine(RESPONSELINE_EMPTY);        // RESPONSELINE_EMPTY
    boxDisplay.writeStatusLine();
    context.ambientOffTimeout = currentMillis + AMBIENT_TIMEOUT;
}

void BoxStateMachine::onExitClosed() {
    // cameraStop() - call from main.cpp
    gpioHal.ambientOff();
}

void BoxStateMachine::onEnterExternal(unsigned long currentMillis) {
    // External command is active
    // UI is blocked
}

void BoxStateMachine::onExitExternal() {
    // External command finished
}

void BoxStateMachine::onEnterBadPass(unsigned long currentMillis) {
    boxDisplay.writeResponseLine(RESPONSELINE_BADPASS);

    if (context.badPasswordDelayFinish <= currentMillis) {
        unsigned long delayMs;
        if (context.badPasswordCount > PASS_ERR_MAXMULTIPLY) {
            delayMs = (unsigned long)PASS_ERR_DELAY * PASS_ERR_MAXMULTIPLY * 1000;
        } else {
            delayMs = (unsigned long)PASS_ERR_DELAY * context.badPasswordCount * 1000;
        }

        context.currentBadPasswordMillis = delayMs;
        context.badPasswordDelayFinish = currentMillis + delayMs;
    }
    
    boxDisplay.writeActionLine(ACTIONLINE_BADPASS);         // display bad password
}

void BoxStateMachine::onExitBadPass() {
    // Nothing specific
}

// Helper methods

bool BoxStateMachine::validatePassword(const char* password) {
    // This is handled by pinStorage.usePin() which returns doorNum if valid, 0 if invalid
    // Actual implementation delegated to pinStorage
    return false;
}

void BoxStateMachine::generatePresenceCode() {
    // Generate 6-digit presence code
    for (int i = 0; i < 6; i++) {
        context.presenceCode[i] = '0' + (rand() % 10);
    }
    context.presenceCode[6] = '\0';
}

bool BoxStateMachine::isDoorOpen(uint8_t doorNum) const {
    // Delegate to gpioHal
    return (gpioHal.readDoorState(doorNum) == 1); // DOOR_OPEN = 1
}

// Check if all doors are closed, i.e. if all physical doors are closed -> logical has to be too
// Thus we can recover from state when anybody in past didn't close the door and we are in a state where we expect all doors to be closed, e.g. after opening and closing a door, or after a timeout, etc.
bool BoxStateMachine::areAllDoorsClosed() const {
    // Check if all doors are closed
    bool doorStates[DOOR_COUNT];
    gpioHal.getOpenDoors(doorStates, DOOR_COUNT);
    for (uint8_t i = 0; i < DOOR_COUNT; i++) {
        if (doorStates[i]) { // If any door is open     
            return false;
        }
    }
    return true;
}

void BoxStateMachine::enableExternal() {
    // Reserved for future websocket UI synchronization.
}

void BoxStateMachine::disableExternal() {
    // Reserved for future websocket UI synchronization.
}
