#ifndef GPIO_HAL_H
#define GPIO_HAL_H

#include <Arduino.h>
#include "timer.h"


#define DOOR_COUNT 20           // Maximum number of physical doors supported
#define DOOR_DELAY 500         // Duration of the pulse to open the door (ms)
#define DOOR_OPENING_TIMEOUT  1000

#define DOOR_UNKNOWN 0
#define DOOR_OPEN 1
#define DOOR_CLOSED 2
#define DOOR_MIXED 3            // For combinations of doors, e.g. adjacent doors operated together
#define DOORSWITCH_CLOSED 1
#define DOORSWITCH_OPEN 0 

#define AMBIENT_ON 1
#define AMBIENT_OFF 0
#define INVALID_PIN 255

typedef struct {
    uint8_t logDoorNum;         // For door combinations, e.g. adjacent doors operated together
    uint8_t gpioPin;
    uint8_t statePin;
    uint8_t extenderNum;        // 0 = onboard GPIO, 1-4 = extender number
    bool lastState;           // Store the last known state of the door for change detection
} DoorMapping;

class GpioHAL {
public:
  // Open a specific door (by number)
  // Returns 1 if door opened successfully, 0 otherwise
  uint8_t openDoor(uint8_t doorNum);

  // Read the state of a door
  // Returns 1 if door is open, 0 if closed
  uint8_t readDoorState(uint8_t doorNum);

  // Set the last state of a door (used for change detection)
  void setDoorLastState(uint8_t doorNum, bool state);

  // Turn ambient light on
  uint8_t ambientOn();

  // Turn ambient light off
  uint8_t ambientOff();

  // Get ambient light level
  uint8_t getAmbient();

  // Lock deactivate (turn off the door lock)
  uint8_t lockDeactivate(uint8_t doorNum);

  // Set the mapping for a specific door
  void setDoorMapping(uint8_t index, uint8_t logDoorNum, uint8_t gpioPin = INVALID_PIN, uint8_t statePin = INVALID_PIN, uint8_t extenderNum = 0);

  // Get the mapping for a specific door
  uint8_t getLogDoorMapping(uint8_t index);

  // Remove the mapping for a specific door
  void removeDoorMapping(uint8_t index);  

  // Initialize GPIO pins based on the current door mappings
  void initializeGpioHAL(TimerManager* timerManager,DoorMapping* initialMappings = nullptr, uint8_t ambientPin = INVALID_PIN, uint8_t mappingSize = 0);

  void setAmbientPin(uint8_t pin); // Set the pin used for ambient light control

  static void doorCallback(); // GPIO interrupt callback for door state changes
  void getOpenDoors(bool* openDoorsArray, uint8_t arraySize); // Get the list of currently open doors, e.g. for display purposes, fills the provided array with 1 for open and 0 for closed, up to the specified array size 

private:

  TimerManager* timerManager = nullptr;           // Pointer to the timer manager for scheduling future tasks
  // Door lock pins (active HIGH)
  static const uint8_t doorLockPins[];
  uint8_t activeDoorNum = 0;                // Number of initialized doors

  // Door state pins (HIGH = open)
  static const uint8_t doorStatePins[DOOR_COUNT];
  // door lock control pins
  static const uint8_t doorRelayPin[DOOR_COUNT];
  // Ambient light control pin
  static uint8_t ambientPin;            // Central switch for whole box ambient light, e.g. for a relay controlling power to the LED strips
  // Pulse duration for door lock
  static const unsigned long doorDelay;
  DoorMapping doorMappings[DOOR_COUNT]; // Example mapping for 20 doors
  static GpioHAL* gpioHalPtr; // Static pointer to the GpioHAL instance for use in static callback

};

#endif // GPIO_HAL_H