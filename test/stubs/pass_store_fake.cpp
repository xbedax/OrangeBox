#include <cstring>

#include "pass_store.h"

bool CredStorage::begin()
{
    return true;
}

uint8_t CredStorage::verifyCred(const char* credValue, CredType credType)
{
    if (credValue == nullptr || credType != CredType::Password) {
        return 0;
    }

    if (std::strcmp(credValue, "1234") == 0 || std::strcmp(credValue, "123456") == 0) {
        return 1;
    }

    return 0;
}

bool CredStorage::usedCred(uint8_t)
{
    return true;
}
