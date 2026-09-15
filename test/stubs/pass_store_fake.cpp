#include <cstring>

#include "pass_store.h"

bool PinStorage::begin()
{
    return true;
}

uint8_t PinStorage::verifyPin(const char* pinValue, PinType pinType)
{
    if (pinValue == nullptr || pinType != PinType::Password) {
        return 0;
    }

    if (std::strcmp(pinValue, "1234") == 0 || std::strcmp(pinValue, "123456") == 0) {
        return 1;
    }

    return 0;
}

bool PinStorage::usedPin(uint8_t)
{
    return true;
}
