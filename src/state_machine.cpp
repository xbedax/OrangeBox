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
    context.retryScanAfterBadCode = false;
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
        case BoxEventType::ScannerInvalidCode:
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
                logger.logPrint(A_PASSWORD_TIMEOUT, LOGAREA_OPERATION, SEVERITY_INFO);
                transitionTo(BoxState::Home, currentMillis);
                break;
            }
            break;

        case BoxState::Scan:
            if (timeoutElapsed(context.scanStartedAt, QR_SCANNER_SCAN_TIMEOUT_MS, currentMillis)) {
                logger.logPrint(A_SCAN_FAILED, LOGAREA_OPERATION, SEVERITY_INFO);
                transitionTo(BoxState::Home, currentMillis);
            }
            break;

        case BoxState::BadPass:
            // Check if bad-password penalty expired
            if (timeoutElapsed(context.badPasswordStartedAt, context.currentBadPasswordMillis, currentMillis)) {
                if (context.state == BoxState::BadPass) {
                    transitionTo(context.retryScanAfterBadCode ? BoxState::Scan : BoxState::Password, currentMillis);
                }
            }
            break;
            
        case BoxState::Presence:
            // Check if presence code expired
            if (timeoutElapsed(context.presenceCodeStartedAt, PRESENCE_CODE_VALIDITY * 1000UL, currentMillis)) {
                logger.logPrint(A_PRESENCE_TIMEOUT, LOGAREA_OPERATION, SEVERITY_INFO);
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
                logger.logPrint(A_DOOR_OPEN_TIMEOUT, LOGAREA_OPERATION, SEVERITY_CRITICAL, context.doorToOpen);
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
            if (context.currentBadPasswordMillis > 0 && !context.retryScanAfterBadCode) {
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
    logger.logPrint(A_STATE_CHANGED, LOGAREA_SYSTEM, SEVERITY_DEBUG, static_cast<int>(oldState), static_cast<int>(newState));
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
    
    logger.logPrint(A_KEYBOARD_EVENT, LOGAREA_SYSTEM, SEVERITY_DEBUG, static_cast<int>(context.state), static_cast<int>(event.eventType)); //>>>
    switch (context.state) {
        case BoxState::Home:
            if (event.eventType == BoxEventType::KeyboardKey1) {
                // Switch to password mode unless a bad-password penalty timeout is still active
                if (context.badPasswordActive && !timeoutElapsed(context.badPasswordStartedAt, context.currentBadPasswordMillis, currentMillis)) {
                    context.retryScanAfterBadCode = false;
                    transitionTo(BoxState::BadPass, currentMillis);
                } else {
                    transitionTo(BoxState::Password, currentMillis);
                }
            } else if (event.eventType == BoxEventType::KeyboardKey2) {
                // Switch to presence mode
                transitionTo(BoxState::Presence, currentMillis);
            } else if (event.eventType == BoxEventType::KeyboardKey3) {
                if (context.badPasswordActive && !timeoutElapsed(context.badPasswordStartedAt, context.currentBadPasswordMillis, currentMillis)) {
                    context.retryScanAfterBadCode = true;
                    transitionTo(BoxState::BadPass, currentMillis);
                } else {
                    transitionTo(BoxState::Scan, currentMillis);
                }
            }
            break;
            
        case BoxState::Password:
            if (event.eventType == BoxEventType::KeyboardEnter) {
                logger.logPrint(A_PASSWORD_ENTERED, LOGAREA_OPERATION, SEVERITY_CRITICAL);
                // Validate password
                context.doorToOpen = credStorage.verifyCred(event.data.keyboardData.password);
                if (context.doorToOpen != DOOR_UNKNOWN) {
                    logger.logPrint(A_PASSWORD_VERIFIED, LOGAREA_OPERATION, SEVERITY_CRITICAL, context.doorToOpen);
                    // Valid PIN - transition to opening
                    context.credVerified = true;
                    context.scannedCredVerified = false;
                    context.badPasswordCount = 0;
                    transitionTo(BoxState::Opening, currentMillis);
                } else {
                    logger.logPrint(A_PASSWORD_BAD, LOGAREA_OPERATION, SEVERITY_CRITICAL);
                    // Invalid PIN - transition to bad password state
                    context.retryScanAfterBadCode = false;
                    if (context.badPasswordCount < UINT8_MAX) ++context.badPasswordCount;
                    transitionTo(BoxState::BadPass, currentMillis);
                }
                break;
            }
            // Fall through to share KeyboardCancel handling with BadPass and Presence.
            
        case BoxState::BadPass:
        case BoxState::Presence:
        case BoxState::Scan:
            if (event.eventType == BoxEventType::KeyboardCancel) {
                if (context.state == BoxState::Presence)
                    logger.logPrint(A_PRESENCE_CANCELED, LOGAREA_OPERATION, SEVERITY_CRITICAL);
                else if (context.state == BoxState::Password || (context.state == BoxState::BadPass && !context.retryScanAfterBadCode))
                    logger.logPrint(A_PASSWORD_CANCELED, LOGAREA_OPERATION, SEVERITY_CRITICAL);
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
    if (timeoutElapsed(context.scanStartedAt, QR_SCANNER_SCAN_TIMEOUT_MS, currentMillis)
        || event.eventType == BoxEventType::ScannerFailed) {
        logger.logPrint(A_SCAN_FAILED, LOGAREA_OPERATION, SEVERITY_INFO);
        transitionTo(BoxState::Home, currentMillis);
        return;
    }
    logger.logPrint(A_CODE_SCANNED, LOGAREA_OPERATION, SEVERITY_CRITICAL);
    if (event.eventType == BoxEventType::ScannerInvalidCode) {
        context.doorToOpen = DOOR_UNKNOWN;
    } else {
        context.doorToOpen = credStorage.verifyCred(event.data.scannerData.cred, CredType::PacketNumber);
    }
    if (context.doorToOpen == DOOR_UNKNOWN) {
        logger.logPrint(A_CODE_UNKNOWN, LOGAREA_OPERATION, SEVERITY_CRITICAL);
        context.retryScanAfterBadCode = true;
        if (context.badPasswordCount < UINT8_MAX) ++context.badPasswordCount;
        transitionTo(BoxState::BadPass, currentMillis);
        return;
    }
    logger.logPrint(A_CODE_VERIFIED, LOGAREA_OPERATION, SEVERITY_CRITICAL, context.doorToOpen);
    context.credVerified = true;
    context.scannedCredVerified = true;
    context.badPasswordCount = 0;
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
    if (event.eventType == BoxEventType::DoorClosed)
        logger.logPrint(A_DOOR_CLOSED, LOGAREA_OPERATION, SEVERITY_CRITICAL, event.data.doorData.doorNum);
    else if (event.eventType == BoxEventType::DoorOpened
             && (event.data.doorData.doorNum != context.doorToOpen
                 || (context.state != BoxState::Opening && context.state != BoxState::Open)))
        logger.logPrint(A_DOOR_OPENED, LOGAREA_OPERATION, SEVERITY_CRITICAL, event.data.doorData.doorNum);
    boxDisplay.setOpenDoorList();
    logger.logPrint(A_SENSOR_EVENT, LOGAREA_SYSTEM, SEVERITY_DEBUG, static_cast<int>(context.state), static_cast<int>(event.eventType), event.data.doorData.doorNum); //>>>.

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
    logger.logPrint(A_TIMER_EVENT, LOGAREA_SYSTEM, SEVERITY_INFO, static_cast<int>(event.eventType));
    switch (event.eventType) {
        case BoxEventType::PasswordTimeout:
            if (context.state == BoxState::Password) {
                logger.logPrint(A_PASSWORD_TIMEOUT, LOGAREA_OPERATION, SEVERITY_INFO);
                transitionTo(BoxState::Home, currentMillis);
            }
            break;

        case BoxEventType::ScanTimeout:
            if (context.state == BoxState::Scan) {
                logger.logPrint(A_SCAN_FAILED, LOGAREA_OPERATION, SEVERITY_INFO);
                transitionTo(BoxState::Home, currentMillis);
            }
            break;
            
        case BoxEventType::PresenceTimeout:
            if (context.state == BoxState::Presence) {
                logger.logPrint(A_PRESENCE_TIMEOUT, LOGAREA_OPERATION, SEVERITY_INFO);
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
    logger.logPrint(A_PASSWORD_STARTED, LOGAREA_OPERATION, SEVERITY_CRITICAL);
    disableExternal();
    context.retryScanAfterBadCode = false;
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
    logger.logPrint(A_SCAN_STARTED, LOGAREA_OPERATION, SEVERITY_INFO);
    static_assert(QR_SCANNER_SCAN_TIMEOUT_MS > 0, "Scanning must have a finite timeout");
    disableExternal();
    context.scanStartedAt = currentMillis;
    context.credVerified = false;
    context.scannedCredVerified = false;
    context.badPasswordActive = false;
    context.currentBadPasswordMillis = 0;
    boxDisplay.writeInfoLine(INFOLINE_CANCEL);
    boxDisplay.writeActionLine(ACTIONLINE_SCAN);
    boxDisplay.setProgressBar(100);
}

void BoxStateMachine::onExitScan() {
    context.scanStartedAt = 0;
}

void BoxStateMachine::onEnterPresence(unsigned long currentMillis) {
    logger.logPrint(A_PRESENCE_STARTED, LOGAREA_OPERATION, SEVERITY_CRITICAL);
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
    // Verification, logging and display writes can outlast the opening timeout.
    // Start the full contact-wait interval when we actually activate the lock.
    currentMillis = millis();
    if (context.doorToOpen != DOOR_UNKNOWN && gpioHal.openDoor(context.doorToOpen) == context.doorToOpen) {
        context.doorOpenStartedAt = currentMillis;
        context.doorOpenDuration = DOOR_OPENING_TIMEOUT;
        gpioHal.ambientOn();
    } else {
        logger.logPrint(A_DOOR_OPEN_FAILED, LOGAREA_OPERATION, SEVERITY_CRITICAL, context.doorToOpen);
        context.doorOpenStartedAt = currentMillis;
        context.doorOpenDuration = 0;
    }
}

void BoxStateMachine::onExitOpening() {
    context.doorOpenStartedAt = 0;
}

void BoxStateMachine::onEnterOpen(unsigned long currentMillis) {
    logger.logPrint(A_DOOR_OPENED, LOGAREA_OPERATION, SEVERITY_CRITICAL, context.doorToOpen);
    bool consumeCred = context.credVerified;
    if (scannerTestKeepCred && context.scannedCredVerified) consumeCred = false;
    if (consumeCred && !credStorage.usedCred(context.doorToOpen)) {
        logger.logPrint(A_CREDENTIAL_CONSUME_FAILED, LOGAREA_ACCESS, SEVERITY_ERROR, context.doorToOpen);
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
    boxDisplay.writeResponseLine(context.retryScanAfterBadCode ? RESPONSELINE_BADSCAN : RESPONSELINE_BADPASS);

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

// Enables external commands, e.g., allowing the UI to control door actions.
void BoxStateMachine::enableExternal() {
    if (lastUIAction != UI_ENABLE_DOOR_CONTROLS) {
        lastUIAction = UI_ENABLE_DOOR_CONTROLS;
        if (onUIUpdate) {
            onUIUpdate(UI_ENABLE_DOOR_CONTROLS);
        }
    }
}

// Disables external commands, e.g. when the box is in a state where it should not be controlled externally (like during door opening).
void BoxStateMachine::disableExternal() {
    if (lastUIAction != UI_DISABLE_DOOR_CONTROLS) {
        lastUIAction = UI_DISABLE_DOOR_CONTROLS;
        if (onUIUpdate) {
            onUIUpdate(UI_DISABLE_DOOR_CONTROLS);
        }
    }
}

// Check if external commands are currently enabled.
bool BoxStateMachine::isExternalEnabled() const {
    return (lastUIAction == UI_ENABLE_DOOR_CONTROLS);
}
