#include "gpio_hal.h"
#include <Arduino.h>
#include "timer.h"

/*
This file implements the GPIO hardware abstraction layer for the box prototype. It defines the GpioHAL class which provides methods to interact with the door locks, read door states, control ambient light, and manage door mappings. The class also includes a static callback method for handling GPIO interrupts when door states change, allowing for real-time notifications to clients about door state changes. The implementation uses the TimerManager to schedule future tasks such as deactivating locks after a delay or notifying clients of state changes.

ToDo: - Implement ambientOn, ambientOff, and getAmbient methods based on the specific hardware setup for ambient light control and sensing.
      - Ensure that the doorCallback method correctly identifies which door's state has changed and schedules notifications accordingly, especially in cases of door combinations.
      - Consider thread safety if the GPIO state can be accessed from multiple contexts (e.g., main loop and interrupt).
      - Add error handling and validation for door numbers and mappings to prevent invalid access.
      - Store Mapping to persistent storage if needed, and load it during initialization.
*/

GpioHAL* GpioHAL::gpioHalPtr = nullptr; // Global pointer to GpioHAL instance for use in static callback
uint8_t GpioHAL::ambientPin = 0; // Example pin for ambient light control, adjust as needed

uint8_t GpioHAL::openDoor(uint8_t doorNum) {
  bool lockActivated = false;
  for (uint8_t i = 0; i < activeDoorNum; i++) {        // Open all doors that match the logDoorNum, allowing for combinations
    if (doorMappings[i].logDoorNum == doorNum && doorMappings[i].gpioPin != INVALID_PIN) {
      // Issue pulse to open the box
      digitalWrite(doorMappings[i].gpioPin, HIGH);
      lockActivated = true;
    }

  }
  if (lockActivated) {
    timerManager->scheduleOnce(DOOR_DELAY, LOCK_DEACTIVATE, String(doorNum).c_str());
    return doorNum; // Return the door number that was attempted to be opened
  }
  return 0; // Return 0 if no door with the specified number was found
} // openDoor

// Read the state of a (logical) door
uint8_t GpioHAL::readDoorState(uint8_t doorNum) {
                                                                //  Combination doors have a number greater then number of physical doors, so we need to check the mapping for the logDoorNum rather than the index, and then check all doors that match the logDoorNum for their state. If all matching doors are in the same state, return that state, otherwise return mixed state. For example, if we have a combination of two adjacent doors operated together, they would share the same logDoorNum in the mapping, and we would check both of their state pins to determine the overall state of the combination.
  uint8_t doorState = DOOR_UNKNOWN;                            // Neutral door state at the beginning, will be set to open or closed based on the first door we check, and if we find any door with a different state, we will return mixed state
  for (uint8_t i = 0; i < activeDoorNum; i++) {
    if (doorMappings[i].logDoorNum == doorNum && doorMappings[i].statePin != INVALID_PIN) { // Check if the door mapping matches the requested logical door number and has a valid state pin
      uint8_t readState = digitalRead(doorMappings[i].statePin);
      doorMappings[i].lastState = readState;                    // Update the last known state of the door for change detection and door state requests speedup, so we don't have to read the pin again if we already know the state from the last read
      if (doorState == DOOR_UNKNOWN) {
          doorState = readState == HIGH ? DOOR_OPEN : DOOR_CLOSED;
      } else if (doorState != (readState == HIGH ? DOOR_OPEN : DOOR_CLOSED)) {
          return DOOR_MIXED; // Mixed state if different doors have different states
      }
    }
  }
  return doorState;
} // readDoorState

void GpioHAL::setDoorLastState(uint8_t doorNum, bool state) {
  for (uint8_t i = 0; i < activeDoorNum; i++) {
    if (doorMappings[i].logDoorNum == doorNum) {
      doorMappings[i].lastState = state;
    }
  }
} // setDoorLastState

// Get the list of currently open PHYSICAL doors, e.g. for display purposes, fills the provided array with 1 for open and 0 for closed, up to the specified array size
void GpioHAL::getOpenDoors(bool* openDoorsArray, uint8_t arraySize) {
  for (uint8_t i = 0; i < arraySize; i++) {
    openDoorsArray[i] = false;
  }
  for (uint8_t i = 0; i < arraySize && i < activeDoorNum; i++) {
    openDoorsArray[i] = doorMappings[i].lastState == DOOR_OPEN ? 1 : 0;
  }
} // getOpenDoors

// Returns the LogDoorNum for specified index, if the index is valid, otherwise returns 0. This can be used to check the mapping for a specific door index, e.g. to determine if it is part of a combination or not. For example, if we have a combination of two adjacent doors operated together, they would share the same logDoorNum in the mapping, and we can use this method to check which logical door number corresponds to a specific physical door index.
uint8_t GpioHAL::getLogDoorMapping(uint8_t index) {
  if (index < activeDoorNum) {
    return doorMappings[index].logDoorNum;
  }
  return 0; // Invalid index
} // getLogDoorMapping  

// GPIO interrupt callback for door state changes -> plans notification of all clients about the change, ideally with the new state of the door, but at least with the information that something changed and clients should update their state by requesting it from the server
// STATIC method, so it can be used as an interrupt callback, but it uses the global pointer to the GpioHAL instance to access the door mappings and timer manager for scheduling notifications. It checks all doors that match the logDoorNum of the changed door for their state, and if any of them have a different state, it considers it a mixed state. It then schedules a notification to clients with the new state of the door combination.
void ARDUINO_ISR_ATTR GpioHAL::doorCallback() {
    if (!gpioHalPtr) {
        return;
    }
    for (uint8_t i = 0; i < gpioHalPtr->activeDoorNum; i++) {
        if (gpioHalPtr->doorMappings[i].statePin != INVALID_PIN) {
            uint8_t currentState = digitalRead(gpioHalPtr->doorMappings[i].statePin);
            if (currentState != gpioHalPtr->doorMappings[i].lastState) {
                gpioHalPtr->doorMappings[i].lastState = currentState;
                gpioHalPtr->timerManager->scheduleOnce(100, NOTIFY_DOOR_CHANGE, String(gpioHalPtr->doorMappings[i].logDoorNum).c_str());
            }
        }
    }
}

uint8_t GpioHAL::lockDeactivate(uint8_t doorNum) {
  bool lockDeactivated = false;
  for (uint8_t i = 0; i < activeDoorNum; i++) {        // Deactivate all doors that match the logDoorNum, allowing for combinations
    if (doorMappings[i].logDoorNum == doorNum && doorMappings[i].gpioPin != INVALID_PIN) {
      digitalWrite(doorMappings[i].gpioPin, LOW);
      lockDeactivated = true;
    }
  }
  if (!lockDeactivated) {
    Serial.println("Error: Invalid door number in lockDeactivate: " + String(doorNum));
    return 0;
  } 
  return doorNum; // Return the door number that was attempted to be deactivated
} // lockDeactivate

uint8_t GpioHAL::ambientOn() {
  if (ambientPin == INVALID_PIN) {
    return AMBIENT_OFF;
  }
  digitalWrite(ambientPin, HIGH); // Turn on ambient light
  // For example, read a light sensor and return 1 if ambient light should be on, otherwise return 0
  return AMBIENT_ON;
} // ambientOn

uint8_t GpioHAL::ambientOff() {
  if (ambientPin == INVALID_PIN) {
    return AMBIENT_OFF;
  }
  digitalWrite(ambientPin, LOW); // Turn off ambient light
  return AMBIENT_OFF;
} // ambientOff

uint8_t GpioHAL::getAmbient() {
  if (ambientPin == INVALID_PIN) {
    return AMBIENT_OFF;
  }
  if (digitalRead(ambientPin) == HIGH) {
    return AMBIENT_ON; // Ambient light is on
  } else {
    return AMBIENT_OFF; // Ambient light is off
  }
} // getAmbient

void GpioHAL::setAmbientPin(uint8_t pin) {
  ambientPin = pin;
  if (ambientPin == INVALID_PIN) {
    return;
  }
  pinMode(ambientPin, OUTPUT);
  digitalWrite(ambientPin, LOW); // Ensure ambient light is initially off
}

void GpioHAL::setDoorMapping(uint8_t index, uint8_t logDoorNum, uint8_t gpioPin, uint8_t statePin, uint8_t extenderNum) {
  if (index < activeDoorNum) {
    doorMappings[index].logDoorNum = logDoorNum;
    if (gpioPin != INVALID_PIN) {
      pinMode(gpioPin, OUTPUT);
      digitalWrite(gpioPin, LOW); // Ensure the door is initially locked
      doorMappings[index].gpioPin = gpioPin;
    }
    if (statePin != INVALID_PIN) {
      pinMode(statePin, INPUT);
      doorMappings[index].statePin = statePin;
    }
    if (extenderNum <= 4) {
      doorMappings[index].extenderNum = extenderNum;
    } else {
      doorMappings[index].extenderNum = 0; // Default to onboard GPIO if invalid extender number is provided
    } 
    
  }
} // setDoorMapping

//Resets mapping to self (e.g. when removing combination and returning to individual door control)
void GpioHAL::removeDoorMapping(uint8_t index) {
  if (index < activeDoorNum) {
    doorMappings[index].logDoorNum = index;
    return;
  }
} // removeDoorMapping

void GpioHAL::initializeGpioHAL(TimerManager* timerManager, DoorMapping* initialMappings, uint8_t ambientPin, uint8_t mappingSize) {
  gpioHalPtr = this; // Set the global pointer to this instance
  // Initialize GPIO pins based on doorMappings and ambientPin
  activeDoorNum = initialMappings != nullptr ? min(mappingSize, (uint8_t)DOOR_COUNT) : 0;
  for (uint8_t i = 0; i < DOOR_COUNT; i++) {
    if (i < activeDoorNum) {
      doorMappings[i] = initialMappings[i]; // Load initial mappings if provided
      if (doorMappings[i].gpioPin != INVALID_PIN) {
        pinMode(doorMappings[i].gpioPin, OUTPUT);
        digitalWrite(doorMappings[i].gpioPin, LOW);
      }
      if (doorMappings[i].statePin != INVALID_PIN) {
        pinMode(doorMappings[i].statePin, INPUT);
      }
    } else {
      doorMappings[i].logDoorNum = i; // Default mapping to self
      doorMappings[i].gpioPin = INVALID_PIN;
      doorMappings[i].statePin = INVALID_PIN;
      doorMappings[i].extenderNum = 0;
      doorMappings[i].lastState = false; // Default to closed
    }
  }
  this->timerManager = timerManager;
  this->ambientPin = ambientPin;
  if (this->ambientPin != INVALID_PIN) {
    pinMode(this->ambientPin, OUTPUT);
    digitalWrite(this->ambientPin, LOW); // Ensure ambient light is initially off
  }
  for (size_t i = 0; i < activeDoorNum; i++)
  { 
    if (doorMappings[i].statePin != INVALID_PIN) { // If the state pin is valid, read the initial state of the door and store it in the mapping for change detection and door state requests speedup, so we don't have to read the pin again if we already know the state from the last read
      doorMappings[i].lastState = digitalRead(doorMappings[i].statePin); // Initialize last known state for change detection
    }
  } 
  Serial.println("GPIO HAL initialized with current door states:");
  for (size_t i = 0; i < activeDoorNum; i++)
  { 
    if (doorMappings[i].statePin != INVALID_PIN) { // If the state pin is valid, attach interrupt for change detection
      attachInterrupt(doorMappings[i].statePin, GpioHAL::doorCallback, CHANGE); // Attach interrupt to each door state pin
    }
  }

} // initialize


