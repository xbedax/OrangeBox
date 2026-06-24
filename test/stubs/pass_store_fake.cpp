#include <cstring>

#include "pass_store.h"

bool PinStorage::begin()
{
    return true;
}

uint8_t PinStorage::usePin(const char* pinValue)
{
    if (pinValue == nullptr) {
        return 0;
    }

    if (std::strcmp(pinValue, "1234") == 0 || std::strcmp(pinValue, "123456") == 0) {
        return 1;
    }

    return 0;
}
