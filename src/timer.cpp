#include "timer.h"
#include "timeout.h"

TimerManager::TimerManager() {
  for (int i = 0; i < MAX_TIMERS; ++i) {
    timers[i].active = false;
    timers[i].startedAt = 0;
    timers[i].interval = 0;
    timers[i].repeat = false;
    timers[i].action = 0;
    strcpy(timers[i].arg, "");
  }
}

int TimerManager::findFreeSlot() {
  for (int i = 0; i < MAX_TIMERS; ++i) {
    if (!timers[i].active) {
      return i;
    }
  }
  return -1;
}

int TimerManager::scheduleOnce(unsigned long delayMs, uint8_t action, const char* arg) {
  int slot = findFreeSlot();
  if (slot < 0 || action == 0) {
    return -1;
  }

  timers[slot].active = true;
  timers[slot].startedAt = millis();
  timers[slot].interval = delayMs;
  timers[slot].repeat = false;
  timers[slot].action = action;
  if (arg != NULL) {
    strcpy(timers[slot].arg, arg);
  } else {
    timers[slot].arg[0] = '\0';
  }
  return slot;
}

int TimerManager::scheduleRepeat(unsigned long intervalMs, uint8_t action, const char* arg) {
  int slot = findFreeSlot();
  if (slot < 0 || action == 0 || intervalMs == 0) {
    return -1;
  }

  timers[slot].active = true;
  timers[slot].startedAt = millis();
  timers[slot].interval = intervalMs;
  timers[slot].repeat = true;
  if (arg != NULL) {
    strcpy(timers[slot].arg, arg);
  } else {
    timers[slot].arg[0] = '\0';
  }
  timers[slot].action = action;
  return slot;
}

bool TimerManager::cancel(int timerId) {
  if (timerId < 0 || timerId >= MAX_TIMERS) {
    return false;
  }

  if (!timers[timerId].active) {
    return false;
  }

  timers[timerId].active = false;
  return true;
}

// Call this in your main loop to check and execute timers
void TimerManager::update(unsigned long now) {
  for (int i = 0; i < MAX_TIMERS; ++i) {
    if (!timers[i].active) {
      continue;
    }

    if (timeoutElapsed(timers[i].startedAt, timers[i].interval, now))  {
      if (actionHandler != nullptr) {
        actionHandler(timers[i].action, timers[i].arg);
      }

      if (timers[i].repeat) {
        timers[i].startedAt = now;
      } else {
        timers[i].active = false;
        continue;
      }
    }
  }
}

void TimerManager::initializeTimerManager(void (*actionHandler)(uint8_t action, const char* arg)) {
  this->actionHandler = actionHandler;
}
