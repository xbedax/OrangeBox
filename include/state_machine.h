#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <Arduino.h>
#include "config.h"

#define UI_DISABLE_DOOR_CONTROLS 1
#define UI_ENABLE_DOOR_CONTROLS 2

/*
 * BoxState - Enumeration of all possible states in the box state machine
 */
enum class BoxState {
    Home,           // Initial state: all closed, ambient inactive, no input
    Password,       // Keyboard password input mode
    Presence,       // Presence code displayed
    Opening,        // Valid box open request received (lock pulse running)
    Open,           // Some door opened
    Closed,         // All doors closed, ambient and camera still running
    External,       // External command active
    BadPass         // Waiting after bad password entered
};

/*
 * BoxEventType - Enumeration of all possible events that can trigger state transitions
 */
enum class BoxEventType {
    // Keyboard events
    KeyboardKey1 = 100,         // Key 1 pressed - switch to password mode (from HOME)
    KeyboardKey2 = 101,         // Key 2 pressed - switch to presence mode (from HOME)
    KeyboardEnter = 102,        // Enter/# pressed - submit password
    KeyboardCancel = 103,       // Cancel/* pressed - go back to HOME
    
    // Web/External events
    WebOpenBox = 200,           // Web request to open door
    
    // Sensor/Timer events
    DoorOpened = 300,           // Door sensor reports opened
    DoorClosed = 301,           // Door sensor reports closed
    PasswordTimeout = 302,      // Password entry timeout expired
    PresenceTimeout = 303,      // Presence code validity timeout
    AmbientTimeout = 304,       // Ambient light deactivation timeout
    DoorOpenTimeout = 305,      // Door opening attempt timeout
    
    // Generic events
    Tick = 10,                  // Regular update tick
    Invalid = 0xFF
};

struct BoxEventData {
    BoxEventType eventType;
    union {
        struct {
            char password[33];              // For KeyboardEnter
        } keyboardData;
        struct {
            uint8_t doorNum;                // For WebOpenBox and door sensor events
            uint8_t doorState;              // For DoorOpened/DoorClosed
        } doorData;
    } data;
};

struct BoxStateContext {
    BoxState state;
    uint8_t doorToOpen;
    uint8_t badPasswordCount;
    uint32_t doorOpenTimeout;
    uint32_t passwordEntryTimeout;
    uint32_t badPasswordDelayFinish;
    uint32_t presenceCodeExpiration;
    uint32_t ambientOffTimeout;
    uint32_t displayActionMillis;
    uint32_t currentPasswordEntryMillis;     // Total duration of current password entry timeout
    uint32_t currentBadPasswordMillis;      // Total duration of current bad password delay
    char presenceCode[7];                   // 6 digits + null terminator
};

/*
 * BoxStateMachine - Event-driven state machine for managing box state transitions
 * 
 * Responsibilities:
 * - Manage state transitions based on events
 * - Execute state-specific actions on entry/exit
 * - Handle event processing and validation
 * 
 * Functions that read state only (not triggering transitions):
 * - handleGetAmbient()
 * - handleGetDoors()
 * - handleGetPins()
 * - handleCapture()
 * 
 * Functions that modify state:
 * - handleOpenBox() -> WebOpenBox event
 * - BoxKeyboard::handleKeyboard() -> KeyboardKey1/2/Enter/Cancel events
 * - Timer callbacks -> *Timeout events
 * - Door sensors -> DoorOpened/DoorClosed events
 */
class BoxStateMachine {
public:
    BoxStateMachine();
    
    // Initialize the state machine
    void initialize();
    
    // Get current state and context
    BoxState getCurrentState() const;
    const BoxStateContext& getContext() const;
    
    // Process an event and perform state transition if applicable
    void processEvent(const BoxEventData& event);
    
    // Update method - to be called from main loop for time-based events
    void update(unsigned long currentMillis);
    
    // Getters for state-dependent data (read-only operations)
    uint8_t getDoorToOpen() const;
    const char* getPresenceCode() const;
    unsigned long getPresenceCodeExpiration() const;
    uint8_t getBadPasswordCount() const;
    unsigned long getBadPasswordDelayFinish() const;
    unsigned long getAmbientOffTimeout() const;
    
    // Callback registration for state transition notifications
    typedef void (*StateChangeCallback)(BoxState oldState, BoxState newState, unsigned long currentMillis);
    typedef void (*UIUpdateCallback)(uint8_t action);
    void setStateChangeCallback(StateChangeCallback callback);
    void setUIUpdateCallback(UIUpdateCallback callback) { onUIUpdate = callback; }
    
private:
    BoxStateContext context;
    
    // Callback
    StateChangeCallback onStateChange;
    UIUpdateCallback onUIUpdate;
    
    // State transition logic
    void transitionTo(BoxState newState, unsigned long currentMillis);
    
    // Event handlers - route to appropriate handler based on event type
    void handleKeyboardEvent(const BoxEventData& event, unsigned long currentMillis);
    void handleWebEvent(const BoxEventData& event, unsigned long currentMillis);
    void handleSensorEvent(const BoxEventData& event, unsigned long currentMillis);
    void handleTimerEvent(const BoxEventData& event, unsigned long currentMillis);
    
    // State entry/exit actions
    void onEnterHome(unsigned long currentMillis);
    void onExitHome();
    void onEnterPassword(unsigned long currentMillis);
    void onExitPassword();
    void onEnterPresence(unsigned long currentMillis);
    void onExitPresence();
    void onEnterOpening(unsigned long currentMillis);
    void onExitOpening();
    void onEnterOpen(unsigned long currentMillis);
    void onExitOpen();
    void onEnterClosed(unsigned long currentMillis);
    void onExitClosed();
    void onEnterExternal(unsigned long currentMillis);
    void onExitExternal();
    void onEnterBadPass(unsigned long currentMillis);
    void onExitBadPass();
    
    // Helper methods
    bool validatePassword(const char* password);
    void generatePresenceCode();
    bool isDisplayRefreshDue(unsigned long currentMillis) const;
    void scheduleNextDisplayRefresh(unsigned long currentMillis);
    bool refreshPeriodicDisplay(unsigned long currentMillis);
    bool isDoorOpen(uint8_t doorNum) const;
    bool areAllDoorsClosed() const;
    void enableExternal();
    void disableExternal();
};

#endif // STATE_MACHINE_H

        