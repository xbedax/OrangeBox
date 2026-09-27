#pragma once
#include <Arduino.h>
#include <stdint.h>

// Millisecond timestamps use the ESP32's 32-bit unsigned counter, also in host tests.
// Check active timeouts regularly: elapsed time must be less than one full wrap
// (about 49.7 days). Zero is a valid start time, never an inactive marker.
inline bool timeoutElapsed(uint32_t timeoutStart, uint32_t timeoutLength, uint32_t now)
{
    return static_cast<uint32_t>(now - timeoutStart) >= timeoutLength;
}

inline bool timeoutElapsed(uint32_t timeoutStart, uint32_t timeoutLength)
{
    return timeoutElapsed(timeoutStart, timeoutLength, static_cast<uint32_t>(millis()));
}

inline uint32_t timeoutRemaining(uint32_t timeoutStart, uint32_t timeoutLength, uint32_t now)
{
    const uint32_t elapsed = static_cast<uint32_t>(now - timeoutStart);
    return elapsed >= timeoutLength ? 0 : timeoutLength - elapsed;
}
