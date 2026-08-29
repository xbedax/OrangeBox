#ifndef BOX_RTC_H
#define BOX_RTC_H

#include "config.h"
#include <Adafruit_I2CDevice.h>
#include <Arduino.h>
#include <RTClib.h>
#include <sys/time.h>
#include <time.h>

class BoxRtc : public RTC_DS3231 {
public:
  explicit BoxRtc(uint8_t i2cAddress = RTC_I2C_ADDRESS);
  ~BoxRtc();

  bool begin(TwoWire* wireInstance = &Wire);
  bool begin(uint8_t i2cAddress, TwoWire* wireInstance);

  bool isAvailable() const;
  bool hasValidRtcTime() const;
  uint8_t getI2cAddress() const;

  void adjust(const DateTime& dt);
  bool lostPower();
  DateTime now();

  bool setSystemTimeFromRtc();
  bool configureNtp(const char* server, long gmtOffsetSec, int daylightOffsetSec, uint32_t syncIntervalMs);
  bool syncRtcFromSystemTime();
  void update(unsigned long currentMillis = 0);

private:
  static void onTimeSync(struct timeval* tv);

  bool isEpochValid(time_t epoch) const;
  bool setSystemClock(time_t epoch);
  bool writeRtc(time_t epoch);
  bool readRegisters(uint8_t startRegister, uint8_t* buffer, size_t length);
  uint8_t readRegister(uint8_t reg);
  void writeRegister(uint8_t reg, uint8_t value);

  static uint8_t bcdToBin(uint8_t value);
  static uint8_t binToBcd(uint8_t value);

  static BoxRtc* activeInstance;

  uint8_t rtcI2cAddress = RTC_I2C_ADDRESS;
  Adafruit_I2CDevice* rtcDevice = nullptr;
  bool available = false;
  bool validRtcTime = false;
  bool ntpConfigured = false;
  volatile bool ntpSyncPending = false;
  uint32_t configuredSyncIntervalMs = RTC_NTP_SYNC_INTERVAL_MS;
  unsigned long lastRtcSyncMillis = 0;
};

#endif // BOX_RTC_H
