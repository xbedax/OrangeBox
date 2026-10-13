#ifndef SSH_DIRECT_CLIENT_H
#define SSH_DIRECT_CLIENT_H

#include "ssh_session.h"
#include <Arduino.h>

class SSHDirectClient {
public:
  SSHDirectClient();
  ~SSHDirectClient();

  SSHDirectClient(const SSHDirectClient &) = delete;
  SSHDirectClient &operator=(const SSHDirectClient &) = delete;

  bool attach(SSHSession *session, LIBSSH2_CHANNEL *channel);
  bool connected();
  bool eof() const { return eof_; }
  int lastError() const { return lastError_; }
  uint16_t lastStopCloseEagainCount() const {
    return lastStopCloseEagainCount_;
  }
  uint16_t lastStopFreeEagainCount() const { return lastStopFreeEagainCount_; }
  uint32_t lastStopDurationMs() const { return lastStopDurationMs_; }
  int lastStopCloseResult() const { return lastStopCloseResult_; }
  int lastStopFreeResult() const { return lastStopFreeResult_; }

  size_t write(const uint8_t *data, size_t len, uint32_t timeoutMs = 5000);
  size_t write(const char *text, uint32_t timeoutMs = 5000);

  int read(uint8_t *buffer, size_t len, uint32_t timeoutMs = 0);
  int read(uint32_t timeoutMs = 0);
  bool readLine(String &out, uint32_t timeoutMs = 5000);

  bool stop(uint32_t timeoutMs = 5000);

private:
  SSHSession *session_ = nullptr;
  LIBSSH2_CHANNEL *channel_ = nullptr;
  bool eof_ = false;
  int lastError_ = 0;
  uint16_t lastStopCloseEagainCount_ = 0;
  uint16_t lastStopFreeEagainCount_ = 0;
  uint32_t lastStopDurationMs_ = 0;
  int lastStopCloseResult_ = 0;
  int lastStopFreeResult_ = 0;
};

#endif // SSH_DIRECT_CLIENT_H
