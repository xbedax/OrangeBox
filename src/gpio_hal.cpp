#include "gpio_hal.h"
#include <Arduino.h>
#include "timer.h"
#include "timeout.h"

// Door contacts are sampled and debounced in loop(), together with state transitions.
// TEMPORARY SOFTWARE DEBOUNCE: the current lock limit switches can bounce.
// TODO(hardware revision): once the PCB has a hardware debouncer, set this to 0
// to disable the software delay. Keep change detection and loop-only processing.
static constexpr uint32_t DOOR_CONTACT_DEBOUNCE_MS = 100;
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
  return DOOR_UNKNOWN; // Return 255 if no door with the specified number was found
} // openDoor

// Read the state of a (logical) door
uint8_t GpioHAL::readDoorState(uint8_t doorNum) {
                                                                //  Combination doors have a number greater then number of physical doors, so we need to check the mapping for the logDoorNum rather than the index, and then check all doors that match the logDoorNum for their state. If all matching doors are in the same state, return that state, otherwise return mixed state. For example, if we have a combination of two adjacent doors operated together, they would share the same logDoorNum in the mapping, and we would check both of their state pins to determine the overall state of the combination.
  uint8_t doorState = DOOR_UNKNOWN;                            // Neutral door state at the beginning, will be set to open or closed based on the first door we check, and if we find any door with a different state, we will return mixed state
  Serial.println("Reading state for logical door number: " + String(doorNum)); //###
  for (uint8_t i = 0; i < activeDoorNum; i++) {
    if (doorMappings[i].logDoorNum == doorNum && doorMappings[i].statePin != INVALID_PIN) { // Check if the door mapping matches the requested logical door number and has a valid state pin
      uint8_t readState = doorMappings[i].lastState;
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
  return DOOR_UNKNOWN; // Invalid index
} // getLogDoorMapping  

void GpioHAL::updateDoorStates(uint32_t now, void (*onChange)(uint8_t)) {
  bool changed[256] = {};
  for (uint8_t i = 0; i < activeDoorNum; ++i) {
    if (doorMappings[i].statePin == INVALID_PIN) continue;
    const bool sample = digitalRead(doorMappings[i].statePin) == HIGH;
    if (sample != sampledDoorState[i]) {
      sampledDoorState[i] = sample;
      doorSampleStartedAt[i] = now;
    }
    if (sample != doorMappings[i].lastState &&
        timeoutElapsed(doorSampleStartedAt[i], DOOR_CONTACT_DEBOUNCE_MS, now)) {
      doorMappings[i].lastState = sample;
      changed[doorMappings[i].logDoorNum] = true;
    }
  }
  // Coalesce physical contacts belonging to the same logical door.
  if (onChange) {
    for (unsigned door = 0; door < DOOR_UNKNOWN; ++door) {
      if (changed[door]) onChange(static_cast<uint8_t>(door));
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
    return DOOR_UNKNOWN; // Return 255 if no door with the specified number was found
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
  Serial.println("GPIO HAL loaded " + String(activeDoorNum) + " active doors:"); //###
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
    } else {
      doorMappings[i].lastState = false; // Default to closed if state pin is invalid
    }
    Serial.print("Door " + String(i) + " (LogDoorNum: " + String(doorMappings[i].logDoorNum) + ") - State Pin: " + String(doorMappings[i].statePin) + ", Last State: " + String(doorMappings[i].lastState)); //###
  } 
  
  for (uint8_t i = 0; i < activeDoorNum; ++i) {
    sampledDoorState[i] = doorMappings[i].lastState;
    doorSampleStartedAt[i] = millis();
  }

} // initialize


