#ifndef TIMER_H
#define TIMER_H

#include <Arduino.h>

#define MAX_TIMERS 10
#define TIMER_INTERVAL 1000 // Check timers every 1 second
#define MAX_TIMER_PARAMETERS 11

#define WATCHDOG_INTERVAL 10000

#define LOCK_TIMEOUT 30000


#define WATCHDOG_SEND 1
#define LOCK_DEACTIVATE 2
#define AMBIENT_DEACTIVATE 4
#define STREAM_DEACTIVATE 8
#define NOTIFY_DOOR_CHANGE 16
#define PASSWORD_TIMEOUT  3


struct Timer {
  bool active;
  uint32_t startedAt;
  unsigned long interval;
  bool repeat;
  uint8_t action;
  char arg[MAX_TIMER_PARAMETERS];
};

class TimerManager {
public:
  TimerManager();

  // Schedule a one-time timer
  int scheduleOnce(unsigned long delayMs, uint8_t action, const char* arg = nullptr);

  // Schedule a repeating timer
  int scheduleRepeat(unsigned long intervalMs, uint8_t action, const char* arg = nullptr);

  // Cancel a timer
  bool cancel(int timerId);

  // Update timers (call this in loop())
  void update() { update(millis()); }
  void update(unsigned long now);

  // Initialize the timer manager (necessary to set actionHandler callback)
  void initializeTimerManager(void (*actionHandler)(uint8_t action, const char* arg) = nullptr);

private:
  Timer timers[MAX_TIMERS];
  void (*actionHandler)(uint8_t action, const char* arg) = nullptr;

  int findFreeSlot();

};

#endif // TIMER_H