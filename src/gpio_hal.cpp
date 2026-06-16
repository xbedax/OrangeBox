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
  if (doorNum >= DOOR_COUNT) {
    return 0; // Invalid door number
  }
  for (uint8_t i = 0; i < DOOR_COUNT; i++) {        // Open all doors that match the logDoorNum, allowing for combinations
    if (doorMappings[i].logDoorNum == doorNum) {
      // Issue pulse to open the box
      digitalWrite(doorMappings[i].gpioPin, HIGH);
      timerManager->scheduleOnce(DOOR_DELAY, LOCK_DEACTIVATE, String(doorMappings[i].logDoorNum).c_str());
    }
  }
  return doorNum; // Return the door number that was attempted to be opened
} // openDoor

// Read the state of a (logical) door
uint8_t GpioHAL::readDoorState(uint8_t doorNum) {
                                                                //  Combination doors have a number greater then number of physical doors, so we need to check the mapping for the logDoorNum rather than the index, and then check all doors that match the logDoorNum for their state. If all matching doors are in the same state, return that state, otherwise return mixed state. For example, if we have a combination of two adjacent doors operated together, they would share the same logDoorNum in the mapping, and we would check both of their state pins to determine the overall state of the combination.
  uint8_t doorState = DOOR_UNKNOWN;                            // Neutral door state at the beginning, will be set to open or closed based on the first door we check, and if we find any door with a different state, we will return mixed state
  if (doorMappings[doorNum].logDoorNum == doorNum) {
    return doorMappings[doorNum].lastState = digitalRead(doorMappings[doorNum].statePin) == HIGH ? DOOR_OPEN : DOOR_CLOSED; // If the door is not part of a combination, just return its state
  }
  for (uint8_t i = 0; i < DOOR_COUNT; i++) {
    if (doorMappings[i].logDoorNum == doorNum) {
        uint8_t readState = digitalRead(doorMappings[i].statePin);
        doorMappings[i].lastState = readState;
        if (doorState == DOOR_UNKNOWN) {
            doorState = readState == HIGH ? DOOR_OPEN : DOOR_CLOSED;
        } else if (doorState != readState) {
            return DOOR_MIXED; // Mixed state if different doors have different states
        }
    }
  }
  return doorState;
} // readDoorState

void GpioHAL::setDoorLastState(uint8_t doorNum, bool state) {
  if (doorNum >= DOOR_COUNT) {
    return; // Invalid door number
  }
  for (uint8_t i = 0; i < DOOR_COUNT; i++) {
    if (doorMappings[i].logDoorNum == doorNum) {
      doorMappings[i].lastState = state;
    }
  }
} // setDoorLastState

// Get the list of currently open PHYSICAL doors, e.g. for display purposes, fills the provided array with 1 for open and 0 for closed, up to the specified array size
void GpioHAL::getOpenDoors(bool* openDoorsArray, uint8_t arraySize) {
  for (uint8_t i = 0; i < arraySize && i < DOOR_COUNT; i++) {
    openDoorsArray[i] = doorMappings[i].lastState == DOOR_OPEN ? 1 : 0;
  }
} // getOpenDoors

uint8_t GpioHAL::getLogDoorMapping(uint8_t index) {
  if (index < DOOR_COUNT) {
    return doorMappings[index].logDoorNum;
  }
  return 0; // Invalid index
} // getLogDoorMapping  

// GPIO interrupt callback for door state changes -> plans notification of all clients about the change, ideally with the new state of the door, but at least with the information that something changed and clients should update their state by requesting it from the server
// STATIC method, so it can be used as an interrupt callback, but it uses the global pointer to the GpioHAL instance to access the door mappings and timer manager for scheduling notifications. It checks all doors that match the logDoorNum of the changed door for their state, and if any of them have a different state, it considers it a mixed state. It then schedules a notification to clients with the new state of the door combination.
void ARDUINO_ISR_ATTR GpioHAL::doorCallback() {
    for (uint8_t i = 0; i < DOOR_COUNT; i++) {
        if (gpioHalPtr && gpioHalPtr->doorMappings[i].statePin != 0) {
            uint8_t currentState = digitalRead(gpioHalPtr->doorMappings[i].statePin);
            if (currentState != gpioHalPtr->doorMappings[i].lastState) {
                gpioHalPtr->doorMappings[i].lastState = currentState;
                gpioHalPtr->timerManager->scheduleOnce(100, NOTIFY_DOOR_CHANGE, String(gpioHalPtr->doorMappings[i].logDoorNum).c_str());
            }
        }
    }
}

uint8_t GpioHAL::lockDeactivate(uint8_t doorNum) {
  if (doorNum >= DOOR_COUNT) {
    return 0; // Invalid door number
  }
  for (uint8_t i = 0; i < DOOR_COUNT; i++) {        // Deactivate all doors that match the logDoorNum, allowing for combinations
    if (doorMappings[i].logDoorNum == doorNum) {
      digitalWrite(doorMappings[i].gpioPin, LOW);
    }
  }
  return doorNum; // Return the door number that was attempted to be deactivated
} // lockDeactivate

uint8_t GpioHAL::ambientOn() {
  digitalWrite(ambientPin, HIGH); // Turn on ambient light
  // For example, read a light sensor and return 1 if ambient light should be on, otherwise return 0
  return 0; // Placeholder implementation
} // ambientOn

uint8_t GpioHAL::ambientOff() {
  digitalWrite(ambientPin, LOW); // Turn off ambient light
  return 0; // Placeholder implementation
} // ambientOff

uint8_t GpioHAL::getAmbient() {
  if (digitalRead(ambientPin) == HIGH) {
    return AMBIENT_ON; // Ambient light is on
  } else {
    return AMBIENT_OFF; // Ambient light is off
  }
} // getAmbient

void GpioHAL::setAmbientPin(uint8_t pin) {
  ambientPin = pin;
  pinMode(ambientPin, OUTPUT);
  digitalWrite(ambientPin, LOW); // Ensure ambient light is initially off
}

void GpioHAL::setDoorMapping(uint8_t index, uint8_t logDoorNum, uint8_t gpioPin, uint8_t statePin, uint8_t extenderNum) {
  if (index < DOOR_COUNT) {
    doorMappings[index].logDoorNum = logDoorNum;
    if (gpioPin != 0) {
      pinMode(gpioPin, OUTPUT);
      digitalWrite(gpioPin, LOW); // Ensure the door is initially locked
      doorMappings[index].gpioPin = gpioPin;
    }
    if (statePin != 0) {
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
  if (index < DOOR_COUNT) {
    doorMappings[index].logDoorNum = index;
    return;
  }
} // removeDoorMapping

void GpioHAL::initializeGpioHAL(TimerManager* timerManager) {
  gpioHalPtr = this; // Set the global pointer to this instance
  // Initialize GPIO pins based on doorMappings and ambientPin
  for (uint8_t i = 0; i < DOOR_COUNT; i++) {
    pinMode(doorMappings[i].gpioPin, OUTPUT);
    digitalWrite(doorMappings[i].gpioPin, LOW); // Ensure all doors are initially locked
    pinMode(doorMappings[i].statePin, INPUT);
  }
  this->timerManager = timerManager;
  pinMode(ambientPin, OUTPUT);
  digitalWrite(ambientPin, LOW); // Ensure ambient light is initially off
  for (size_t i = 0; i < DOOR_COUNT; i++)
  { 
    if (doorMappings[i].statePin != 0) {
      doorMappings[i].lastState = digitalRead(doorMappings[i].statePin); // Initialize last known state for change detection
    }
  } 
  Serial.println("GPIO HAL initialized with current door states:");
  for (size_t i = 0; i < DOOR_COUNT; i++)
  { 
    if (doorMappings[i].statePin != 0) {
      attachInterrupt(doorMappings[i].statePin, GpioHAL::doorCallback, CHANGE); // Attach interrupt to each door state pin
    }
  }

} // initialize


