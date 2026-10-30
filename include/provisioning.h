#pragma once

#include <Arduino.h>
class BoxConfigStore;
class BoxDisplay;
class BoxKeyboard;
class HardwareSerial;

// Runs before the operating state machine, GPIO, network services and VPN start.
void runProvisioning(BoxConfigStore& store, BoxDisplay& display,
                     BoxKeyboard& keyboard, HardwareSerial& uart);
