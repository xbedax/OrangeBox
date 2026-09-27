#include <cassert>
#include <iostream>
#include "timeout.h"
#include "timer.h"

static unsigned fired[4] = {};
static void onTimer(uint8_t action, const char*) { ++fired[action]; }

int main()
{
    const uint32_t start = UINT32_MAX - 9;
    assert(!timeoutElapsed(start, 20, UINT32_MAX));
    assert(!timeoutElapsed(start, 20, 9));
    assert(timeoutElapsed(start, 20, 10));
    assert(timeoutElapsed(start, 20, 11));
    assert(timeoutRemaining(start, 20, 0) == 10);
    assert(timeoutRemaining(start, 20, 10) == 0);
    assert(timeoutRemaining(start, 20, 11) == 0);
    assert(timeoutElapsed(0, 0, 0));
    assert(!timeoutElapsed(0, UINT32_MAX, UINT32_MAX - 1));
    assert(timeoutElapsed(0, UINT32_MAX, UINT32_MAX));
    testStubMillis = 10;
    assert(timeoutElapsed(start, 20));

    TimerManager timers;
    timers.initializeTimerManager(onTimer);
    testStubMillis = start;
    timers.scheduleOnce(10, 1); // Deadline is exactly zero.
    timers.scheduleOnce(5, 2);  // Must run first despite the wrapped deadline above.
    const int repeated = timers.scheduleRepeat(20, 3);
    timers.update(UINT32_MAX - 5);
    assert(fired[1] == 0 && fired[2] == 0 && fired[3] == 0);
    timers.update(UINT32_MAX - 4);
    assert(fired[2] == 1 && fired[1] == 0);
    // Explicit zero must be honored even when millis() returns a different value.
    timers.update(0);
    assert(fired[1] == 1 && fired[3] == 0);
    timers.update(9);
    assert(fired[3] == 0);
    timers.update(10);
    assert(fired[3] == 1);
    timers.update(29);
    assert(fired[3] == 1);
    timers.update(30);
    assert(fired[3] == 2);
    assert(timers.cancel(repeated));
    timers.update(50);
    assert(fired[3] == 2);

    testStubMillis = 0;
    timers.scheduleOnce(0, 1);
    timers.update();
    assert(fired[1] == 2);
    const int cancelled = timers.scheduleOnce(10, 2);
    assert(timers.cancel(cancelled));
    timers.scheduleOnce(20, 1);
    timers.update(20);
    assert(fired[1] == 3 && fired[2] == 1);
    std::cout << "Timeout and TimerManager rollover tests passed\n";
}
