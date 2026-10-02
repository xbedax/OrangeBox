#include "state_machine.h"
#include "timeout.h"
#include "box_display.h"
#include "gpio_hal.h"
#include "pass_store.h"
#include "timer.h"
#include "config.h"
#include "logger.h"
#include <cstring>

// Forward declarations of external objects (to be linked from main.cpp)
extern BoxDisplay boxDisplay;
extern GpioHAL gpioHal;
extern CredStorage credStorage;
extern TimerManager timerManager;

// Constants from config
//#define DOOR_OPENING_TIMEOUT 5000           // ms - from main.cpp
//#define AMBIENT_TIMEOUT 10000               // ms - after door closes

BoxStateMachine::BoxStateMachine() : onStateChange(nullptr) {
    memset(&context, 0, sizeof(BoxStateContext));
    context.state = BoxState::Home;
    context.doorToOpen = 0;
    context.credVerified = false;
    context.badPasswordCount = 0;
}

void BoxStateMachine::initialize() {
    lastUIAction = 0;
    unsigned long currentMillis = millis();
    context.state = BoxState::Home;
    context.doorToOpen = 0;
    context.credVerified = false;
    context.badPasswordCount = 0;
    context.doorOpenStartedAt = 0;
    context.passwordEntryStartedAt = 0;
    context.scanStartedAt = 0;
    context.scannedCredVerified = false;
    context.badPasswordStartedAt = 0;
    context.presenceCodeStartedAt = 0;
    context.presenceCodeActive = false;
    context.displayRefreshActive = false;
    context.ambientOffStartedAt = 0;
    context.displayRefreshStartedAt = 0;
    context.currentPasswordEntryMillis = 0;
    context.currentBadPasswordMillis = 0;
    context.badPasswordActive = false;

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
        case BoxEventType::KeyboardKey3:
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
        case BoxEventType::ScanTimeout:
            handleTimerEvent(event, currentMillis);
            break;

        case BoxEventType::ScannerCodeReceived:
        case BoxEventType::ScannerFailed:
            handleScannerEvent(event, currentMillis);
            break;
            
        default:
            break;
    }
} // processEvent

void BoxStateMachine::update(unsigned long currentMillis) {
    if (context.badPasswordActive && timeoutElapsed(context.badPasswordStartedAt, context.currentBadPasswordMillis, currentMillis)) {
        context.badPasswordActive = false;
    }
    if (context.presenceCodeActive && timeoutElapsed(context.presenceCodeStartedAt, PRESENCE_CODE_VALIDITY * 1000UL, currentMillis)) {
        context.presenceCodeActive = false;
    }
    // Time-based state transitions and periodic updates
    if (isDisplayRefreshDue(currentMillis)) {
        if (refreshPeriodicDisplay(currentMillis)) {
            scheduleNextDisplayRefresh(currentMillis);
        } else {
            context.displayRefreshActive = false;
        }
    }

    switch (context.state) {
        case BoxState::Home:
            break;

        case BoxState::Password:
            if (timeoutElapsed(context.passwordEntryStartedAt, context.currentPasswordEntryMillis, currentMillis)) {
                transitionTo(BoxState::Home, currentMillis);
                break;
            }
            break;

        case BoxState::Scan:
            if (timeoutElapsed(context.scanStartedAt, QR_SCANNER_SCAN_TIMEOUT_MS, currentMillis)) {
                transitionTo(BoxState::Home, currentMillis);
            }
            break;

        case BoxState::BadPass:
            // Check if bad-password penalty expired
            if (timeoutElapsed(context.badPasswordStartedAt, context.currentBadPasswordMillis, currentMillis)) {
                if (context.state == BoxState::BadPass) {
                    transitionTo(BoxState::Password, currentMillis);
                }
            }
            break;
            
        case BoxState::Presence:
            // Check if presence code expired
            if (timeoutElapsed(context.presenceCodeStartedAt, PRESENCE_CODE_VALIDITY * 1000UL, currentMillis)) {
                transitionTo(BoxState::Home, currentMillis);
            }
            break;
            
        case BoxState::Opening:
            if (gpioHal.readDoorState(context.doorToOpen) == DOOR_OPEN) {
                transitionTo(BoxState::Open, currentMillis);
                break;
            }

            // Check if door opening timeout expired
            if (timeoutElapsed(context.doorOpenStartedAt, context.doorOpenDuration, currentMillis)) {
                logger.logPrint(SEVERITY_ERROR, "Error - door " + String(context.doorToOpen) + " not opened", BOX_HOST_NAME, LOGAREA_ACCESS);
                transitionTo(BoxState::Home, currentMillis);
            }
            break;
            
        case BoxState::Closed:
            // Check if ambient light should be turned off
            if (timeoutElapsed(context.ambientOffStartedAt, AMBIENT_TIMEOUT, currentMillis)) {
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

bool BoxStateMachine::isPresenceCodeValid() const {
    return context.presenceCodeActive
        && !timeoutElapsed(context.presenceCodeStartedAt, PRESENCE_CODE_VALIDITY * 1000UL);
}

uint8_t BoxStateMachine::getBadPasswordCount() const {
    return context.badPasswordCount;
}

void BoxStateMachine::setStateChangeCallback(StateChangeCallback callback) {
    onStateChange = callback;
}

bool BoxStateMachine::isDisplayRefreshDue(unsigned long currentMillis) const {
    return context.displayRefreshActive
        && timeoutElapsed(context.displayRefreshStartedAt, ACTIONLINE_REFRESH_INTERVAL, currentMillis);
}

void BoxStateMachine::scheduleNextDisplayRefresh(unsigned long currentMillis) {
    context.displayRefreshStartedAt = currentMillis;
    context.displayRefreshActive = true;
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
                const uint32_t remaining = timeoutRemaining(context.passwordEntryStartedAt, context.currentPasswordEntryMillis, currentMillis);
                uint8_t progress = (uint8_t)((remaining * 100UL) / context.currentPasswordEntryMillis);
                boxDisplay.setProgressBar(progress);
            }
            return true;

        case BoxState::BadPass:
            boxDisplay.writeActionLine(ACTIONLINE_BADPASS);
            if (context.currentBadPasswordMillis > 0) {
                const uint32_t remaining = timeoutRemaining(context.badPasswordStartedAt, context.currentBadPasswordMillis, currentMillis);
                uint8_t progress = (uint8_t)((remaining * 100UL) / context.currentBadPasswordMillis);
                boxDisplay.setProgressBar(progress);
            }
            return true;

        case BoxState::Scan: {
            const uint32_t remaining = timeoutRemaining(context.scanStartedAt, QR_SCANNER_SCAN_TIMEOUT_MS, currentMillis);
            boxDisplay.setProgressBar(static_cast<uint8_t>((uint64_t(remaining) * 100) / QR_SCANNER_SCAN_TIMEOUT_MS));
            return true;
        }

        case BoxState::Presence:
            {
                const uint32_t remaining = timeoutRemaining(context.presenceCodeStartedAt, PRESENCE_CODE_VALIDITY * 1000UL, currentMillis);
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
        case BoxState::Scan:
            onExitScan();
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
        case BoxState::Scan:
            onEnterScan(currentMillis);
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
                if (context.badPasswordActive && !timeoutElapsed(context.badPasswordStartedAt, context.currentBadPasswordMillis, currentMillis)) {
                    transitionTo(BoxState::BadPass, currentMillis);
                } else {
                    transitionTo(BoxState::Password, currentMillis);
                }
            } else if (event.eventType == BoxEventType::KeyboardKey2) {
                // Switch to presence mode
                transitionTo(BoxState::Presence, currentMillis);
            } else if (event.eventType == BoxEventType::KeyboardKey3) {
                transitionTo(BoxState::Scan, currentMillis);
            }
            break;
            
        case BoxState::Password:
            if (event.eventType == BoxEventType::KeyboardEnter) {
                // Validate password
                context.doorToOpen = credStorage.verifyCred(event.data.keyboardData.password);
                if (context.doorToOpen != DOOR_UNKNOWN) {
                    // Valid PIN - transition to opening
                    context.credVerified = true;
                    context.scannedCredVerified = false;
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
        case BoxState::Scan:
            if (event.eventType == BoxEventType::KeyboardCancel) {
                // Return to home
                transitionTo(BoxState::Home, currentMillis);
            }
            
            break;
            
        default:
            break;
    }
}

void BoxStateMachine::handleScannerEvent(const BoxEventData& event, unsigned long currentMillis) {
    if (context.state != BoxState::Scan) return;
    if (event.eventType == BoxEventType::ScannerFailed
        || timeoutElapsed(context.scanStartedAt, QR_SCANNER_SCAN_TIMEOUT_MS, currentMillis)) {
        transitionTo(BoxState::Home, currentMillis);
        return;
    }
    context.doorToOpen = credStorage.verifyCred(event.data.scannerData.cred, CredType::PacketNumber);
    if (context.doorToOpen == DOOR_UNKNOWN) {
        transitionTo(BoxState::Home, currentMillis);
        return;
    }
    context.credVerified = true;
    context.scannedCredVerified = true;
    transitionTo(BoxState::Opening, currentMillis);
}

void BoxStateMachine::handleWebEvent(const BoxEventData& event, unsigned long currentMillis) {
    if (event.eventType == BoxEventType::WebOpenBox) {
        // Web request to open door
        context.doorToOpen = event.data.doorData.doorNum;
        context.credVerified = false;
        context.scannedCredVerified = false;
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

        case BoxEventType::ScanTimeout:
            if (context.state == BoxState::Scan) transitionTo(BoxState::Home, currentMillis);
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
    // Opening can time out without ever passing through Closed.
    gpioHal.ambientOff();
    // TODO: When camera control is implemented, stop it on entry to Home as well.
    context.credVerified = false;
    enableExternal();
    context.scannedCredVerified = false;
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
    context.badPasswordStartedAt = 0;
    context.currentBadPasswordMillis = 0;
    context.badPasswordActive = false;
    context.currentPasswordEntryMillis = (unsigned long)PASS_ENTRY_TIMEOUT * 1000UL;
    context.passwordEntryStartedAt = currentMillis;
    boxDisplay.setProgressBar(100);
    
}

void BoxStateMachine::onExitPassword() {
    context.passwordEntryStartedAt = 0;
    context.currentPasswordEntryMillis = 0;
}

void BoxStateMachine::onEnterScan(unsigned long currentMillis) {
    static_assert(QR_SCANNER_SCAN_TIMEOUT_MS > 0, "Scanning must have a finite timeout");
    disableExternal();
    context.scanStartedAt = currentMillis;
    context.credVerified = false;
    context.scannedCredVerified = false;
    boxDisplay.writeInfoLine(INFOLINE_CANCEL);
    boxDisplay.writeActionLine(ACTIONLINE_SCAN);
    boxDisplay.setProgressBar(100);
}

void BoxStateMachine::onExitScan() {
    context.scanStartedAt = 0;
}

void BoxStateMachine::onEnterPresence(unsigned long currentMillis) {
    generatePresenceCode();
    context.presenceCodeStartedAt = currentMillis;
    context.presenceCodeActive = true;
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
    if (context.doorToOpen != DOOR_UNKNOWN && gpioHal.openDoor(context.doorToOpen) == context.doorToOpen) {
        context.doorOpenStartedAt = currentMillis;
        context.doorOpenDuration = DOOR_OPENING_TIMEOUT;
        gpioHal.ambientOn();
    } else {
        logger.logPrint(SEVERITY_ERROR, "Error - door " + String(context.doorToOpen) + " cannot be opened", BOX_HOST_NAME, LOGAREA_ACCESS);
        context.doorOpenStartedAt = currentMillis;
        context.doorOpenDuration = 0;
    }
}

void BoxStateMachine::onExitOpening() {
    context.doorOpenStartedAt = 0;
}

void BoxStateMachine::onEnterOpen(unsigned long currentMillis) {
    bool consumeCred = context.credVerified;
#ifdef QR_SCANNER_TEST_KEEP_CRED
    if (context.scannedCredVerified) consumeCred = false;
#endif
    if (consumeCred && !credStorage.usedCred(context.doorToOpen)) {
        logger.logPrint(SEVERITY_ERROR, "Error - verified credential for door " + String(context.doorToOpen) + " could not be consumed", BOX_HOST_NAME, LOGAREA_ACCESS);
    }
    context.credVerified = false;
    context.scannedCredVerified = false;
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
    context.ambientOffStartedAt = currentMillis;
    context.doorToOpen = DOOR_UNKNOWN;
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

    if (!context.badPasswordActive || timeoutElapsed(context.badPasswordStartedAt, context.currentBadPasswordMillis, currentMillis)) {
        unsigned long delayMs;
        if (context.badPasswordCount > PASS_ERR_MAXMULTIPLY) {
            delayMs = (unsigned long)PASS_ERR_DELAY * PASS_ERR_MAXMULTIPLY * 1000;
        } else {
            delayMs = (unsigned long)PASS_ERR_DELAY * context.badPasswordCount * 1000;
        }

        context.currentBadPasswordMillis = delayMs;
        context.badPasswordStartedAt = currentMillis;
        context.badPasswordActive = true;
    }
    
    boxDisplay.writeActionLine(ACTIONLINE_BADPASS);         // display bad password
}

void BoxStateMachine::onExitBadPass() {
    // Nothing specific
}

// Helper methods

bool BoxStateMachine::validatePassword(const char* password) {
    // This is handled by credStorage.verifyCred() which returns doorNum if valid, 0 if invalid
    // Actual implementation delegated to credStorage
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

//
void BoxStateMachine::enableExternal() {
    // Reserved for future websocket UI synchronization.
    if(onUIUpdate && lastUIAction != UI_ENABLE_DOOR_CONTROLS) {
        lastUIAction = UI_ENABLE_DOOR_CONTROLS;
        onUIUpdate(UI_ENABLE_DOOR_CONTROLS);
    }
}

// Disables external commands, e.g. when the box is in a state where it should not be controlled externally (like during door opening).
void BoxStateMachine::disableExternal() {
    if(onUIUpdate && lastUIAction != UI_DISABLE_DOOR_CONTROLS) {
        lastUIAction = UI_DISABLE_DOOR_CONTROLS;
        onUIUpdate(UI_DISABLE_DOOR_CONTROLS);
    }
}
