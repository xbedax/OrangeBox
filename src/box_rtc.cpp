#include "box_rtc.h"

#include <esp_sntp.h>

#define DS3231_TIME_REGISTER 0x00
#define DS3231_STATUS_REGISTER 0x0F
#define DS3231_OSCILLATOR_STOP_FLAG 0x80

BoxRtc* BoxRtc::activeInstance = nullptr;

BoxRtc::BoxRtc(uint8_t i2cAddress)
  : rtcI2cAddress(i2cAddress)
{
}

BoxRtc::~BoxRtc()
{
  if (rtcDevice != nullptr) {
    delete rtcDevice;
    rtcDevice = nullptr;
  }
}

bool BoxRtc::begin(TwoWire* wireInstance)
{
  return begin(rtcI2cAddress, wireInstance);
}

bool BoxRtc::begin(uint8_t i2cAddress, TwoWire* wireInstance)
{
  available = false;
  validRtcTime = false;
  rtcI2cAddress = i2cAddress;

  if (wireInstance == nullptr || rtcI2cAddress > 0x7F) {
    return false;
  }

  Adafruit_I2CDevice* newDevice = new Adafruit_I2CDevice(rtcI2cAddress, wireInstance);
  if (newDevice == nullptr) {
    return false;
  }

  if (!newDevice->begin()) {
    delete newDevice;
    return false;
  }

  if (rtcDevice != nullptr) {
    delete rtcDevice;
  }
  rtcDevice = newDevice;
  available = true;

  if (lostPower()) {
    return true;
  }

  DateTime rtcNow = now();
  validRtcTime = isEpochValid(rtcNow.unixtime());
  return true;
}

bool BoxRtc::isAvailable() const
{
  return available;
}

bool BoxRtc::hasValidRtcTime() const
{
  return validRtcTime;
}

uint8_t BoxRtc::getI2cAddress() const
{
  return rtcI2cAddress;
}

void BoxRtc::adjust(const DateTime& dt)
{
  if (!available || rtcDevice == nullptr) {
    return;
  }

  uint8_t buffer[8] = {
    DS3231_TIME_REGISTER,
    binToBcd(dt.second()),
    binToBcd(dt.minute()),
    binToBcd(dt.hour()),
    binToBcd(RTC_DS3231::dowToDS3231(dt.dayOfTheWeek())),
    binToBcd(dt.day()),
    binToBcd(dt.month()),
    binToBcd(dt.year() - 2000U)
  };
  rtcDevice->write(buffer, sizeof(buffer));

  uint8_t status = readRegister(DS3231_STATUS_REGISTER);
  status &= ~DS3231_OSCILLATOR_STOP_FLAG;
  writeRegister(DS3231_STATUS_REGISTER, status);
}

bool BoxRtc::lostPower()
{
  if (!available || rtcDevice == nullptr) {
    return true;
  }

  return (readRegister(DS3231_STATUS_REGISTER) & DS3231_OSCILLATOR_STOP_FLAG) != 0;
}

DateTime BoxRtc::now()
{
  uint8_t buffer[7] = {0};
  if (!readRegisters(DS3231_TIME_REGISTER, buffer, sizeof(buffer))) {
    return DateTime(2000, 1, 1, 0, 0, 0);
  }

  return DateTime(
    bcdToBin(buffer[6]) + 2000U,
    bcdToBin(buffer[5] & 0x7F),
    bcdToBin(buffer[4]),
    bcdToBin(buffer[2]),
    bcdToBin(buffer[1]),
    bcdToBin(buffer[0] & 0x7F)
  );
}

bool BoxRtc::setSystemTimeFromRtc()
{
  if (!available || lostPower()) {
    return false;
  }

  DateTime rtcNow = now();
  time_t rtcEpoch = static_cast<time_t>(rtcNow.unixtime());
  if (!isEpochValid(rtcEpoch)) {
    validRtcTime = false;
    return false;
  }

  if (!setSystemClock(rtcEpoch)) {
    return false;
  }

  validRtcTime = true;
  return true;
}

bool BoxRtc::configureNtp(const char* server, long gmtOffsetSec, int daylightOffsetSec, uint32_t syncIntervalMs)
{
  if (server == nullptr || server[0] == '\0') {
    return false;
  }

  activeInstance = this;
  configuredSyncIntervalMs = syncIntervalMs;
  sntp_set_time_sync_notification_cb(BoxRtc::onTimeSync);
  if (configuredSyncIntervalMs > 0) {
    sntp_set_sync_interval(configuredSyncIntervalMs);
  }

  configTime(gmtOffsetSec, daylightOffsetSec, server);
  ntpConfigured = true;
  return true;
}

bool BoxRtc::syncRtcFromSystemTime()
{
  time_t currentTime = time(nullptr);
  return writeRtc(currentTime);
}

void BoxRtc::update(unsigned long currentMillis)
{
  if (!ntpConfigured || !ntpSyncPending) {
    return;
  }

  ntpSyncPending = false;
  if (!syncRtcFromSystemTime()) {
    return;
  }

  lastRtcSyncMillis = currentMillis == 0 ? millis() : currentMillis;
}

void BoxRtc::onTimeSync(struct timeval* tv)
{
  if (activeInstance == nullptr || tv == nullptr) {
    return;
  }

  activeInstance->ntpSyncPending = true;
}

bool BoxRtc::isEpochValid(time_t epoch) const
{
  return epoch >= static_cast<time_t>(RTC_MIN_VALID_UNIX_TIME);
}

bool BoxRtc::setSystemClock(time_t epoch)
{
  if (!isEpochValid(epoch)) {
    return false;
  }

  struct timeval tv;
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  return settimeofday(&tv, nullptr) == 0;
}

bool BoxRtc::writeRtc(time_t epoch)
{
  if (!available || !isEpochValid(epoch)) {
    return false;
  }

  adjust(DateTime(static_cast<uint32_t>(epoch)));
  validRtcTime = true;
  return true;
}

bool BoxRtc::readRegisters(uint8_t startRegister, uint8_t* buffer, size_t length)
{
  if (rtcDevice == nullptr || buffer == nullptr || length == 0) {
    return false;
  }

  return rtcDevice->write_then_read(&startRegister, 1, buffer, length);
}

uint8_t BoxRtc::readRegister(uint8_t reg)
{
  uint8_t value = 0;
  readRegisters(reg, &value, 1);
  return value;
}

void BoxRtc::writeRegister(uint8_t reg, uint8_t value)
{
  if (rtcDevice == nullptr) {
    return;
  }

  uint8_t buffer[2] = {reg, value};
  rtcDevice->write(buffer, sizeof(buffer));
}

uint8_t BoxRtc::bcdToBin(uint8_t value)
{
  return value - 6 * (value >> 4);
}

uint8_t BoxRtc::binToBcd(uint8_t value)
{
  return value + 6 * (value / 10);
}
