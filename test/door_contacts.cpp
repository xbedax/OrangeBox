#include <cassert>
#include <iostream>
#include <vector>
#include "gpio_hal.h"

static std::vector<uint8_t> changes;
static void changed(uint8_t door) { changes.push_back(door); }

int main() {
    for (uint32_t start : {uint32_t(0), UINT32_MAX - 50U}) {
        testStubMillis = start;
        testStubPinState[10] = LOW;
        testStubPinState[11] = LOW;
        TimerManager timers;
        GpioHAL gpio;
        DoorMapping mappings[] = {{0, 20, 10, 0, false}, {0, 21, 11, 0, false}};
        gpio.initializeGpioHAL(&timers, mappings, INVALID_PIN, 2);
        changes.clear();
        gpio.updateDoorStates(start, changed);
        assert(changes.empty());
        // Contact bounce must neither publish nor change the state seen by the FSM.
        for (uint32_t t = 10; t <= 90; t += 10) {
            testStubPinState[10] = (t / 10) % 2 ? HIGH : LOW;
            gpio.updateDoorStates(start + t, changed);
            assert(gpio.readDoorState(0) == DOOR_CLOSED);
        }
        testStubPinState[11] = HIGH;
        gpio.updateDoorStates(start + 90, changed);
        gpio.updateDoorStates(start + 189, changed);
        assert(changes.empty());
        gpio.updateDoorStates(start + 190, changed);
        assert(changes.size() == 1 && changes[0] == 0);
        assert(gpio.readDoorState(0) == DOOR_OPEN);
        for (uint32_t t = 200; t < 500; t += 10) gpio.updateDoorStates(start + t, changed);
        assert(changes.size() == 1);
        testStubPinState[10] = LOW;
        testStubPinState[11] = LOW;
        gpio.updateDoorStates(start + 500, changed);
        gpio.updateDoorStates(start + 599, changed);
        assert(gpio.readDoorState(0) == DOOR_OPEN);
        gpio.updateDoorStates(start + 600, changed);
        assert(changes.size() == 2 && changes[1] == 0);
        assert(gpio.readDoorState(0) == DOOR_CLOSED);
    }
    std::cout << "Door contact debounce tests passed\n";
}
