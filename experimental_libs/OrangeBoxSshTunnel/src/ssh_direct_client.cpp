#include "ssh_direct_client.h"
#include <cstring>

SSHDirectClient::SSHDirectClient() {}

SSHDirectClient::~SSHDirectClient() { stop(5000); }

bool SSHDirectClient::attach(SSHSession *session, LIBSSH2_CHANNEL *channel) {
  stop();
  if (session == nullptr || channel == nullptr) {
    return false;
  }
  session_ = session;
  channel_ = channel;
  eof_ = false;
  lastError_ = 0;
  return true;
}

bool SSHDirectClient::connected() {
  if (session_ == nullptr || channel_ == nullptr || eof_ ||
      !session_->isConnected()) {
    return false;
  }

  if (session_->lock(pdMS_TO_TICKS(50))) {
    if (libssh2_channel_eof(channel_)) {
      eof_ = true;
    }
    session_->unlock();
  }
  return !eof_;
}

size_t SSHDirectClient::write(const uint8_t *data, size_t len,
                              uint32_t timeoutMs) {
  if (session_ == nullptr || channel_ == nullptr || data == nullptr ||
      len == 0) {
    return 0;
  }

  size_t writtenTotal = 0;
  uint32_t start = millis();
  uint32_t deadline = start + timeoutMs;

  while (writtenTotal < len &&
         (timeoutMs == 0 ||
          static_cast<int32_t>(millis() - deadline) < 0)) {
    if (!session_->lock(pdMS_TO_TICKS(50))) {
      delay(2);
      continue;
    }

    ssize_t written =
        libssh2_channel_write(channel_,
                              reinterpret_cast<const char *>(data) +
                                  writtenTotal,
                              len - writtenTotal);
    if (written > 0) {
      writtenTotal += static_cast<size_t>(written);
      lastError_ = 0;
      session_->unlock();
      continue;
    }

    if (written == LIBSSH2_ERROR_EAGAIN) {
      session_->unlock();
      delay(5);
      continue;
    }

    lastError_ = static_cast<int>(written);
    session_->unlock();
    break;
  }

  return writtenTotal;
}

size_t SSHDirectClient::write(const char *text, uint32_t timeoutMs) {
  if (text == nullptr) {
    return 0;
  }
  return write(reinterpret_cast<const uint8_t *>(text), strlen(text),
               timeoutMs);
}

int SSHDirectClient::read(uint8_t *buffer, size_t len, uint32_t timeoutMs) {
  if (session_ == nullptr || channel_ == nullptr || buffer == nullptr ||
      len == 0) {
    return -1;
  }

  uint32_t deadline = millis() + timeoutMs;
  while (timeoutMs == 0 || static_cast<int32_t>(millis() - deadline) < 0) {
    if (!session_->lock(pdMS_TO_TICKS(50))) {
      delay(2);
      continue;
    }

    int rc = libssh2_channel_read(channel_, reinterpret_cast<char *>(buffer),
                                  len);
    if (rc > 0) {
      lastError_ = 0;
      session_->unlock();
      return rc;
    }

    if (rc == 0) {
      eof_ = true;
      session_->unlock();
      return 0;
    }

    if (rc == LIBSSH2_ERROR_EAGAIN) {
      session_->unlock();
      if (timeoutMs == 0) {
        return 0;
      }
      delay(5);
      continue;
    }

    lastError_ = rc;
    eof_ = true;
    session_->unlock();
    return -1;
  }

  return 0;
}

int SSHDirectClient::read(uint32_t timeoutMs) {
  uint8_t value = 0;
  int rc = read(&value, 1, timeoutMs);
  return rc == 1 ? value : rc;
}

bool SSHDirectClient::readLine(String &out, uint32_t timeoutMs) {
  out = "";
  uint32_t deadline = millis() + timeoutMs;
  while (timeoutMs == 0 || static_cast<int32_t>(millis() - deadline) < 0) {
    int c = read(50);
    if (c > 0) {
      if (c == '\n') {
        return true;
      }
      if (c != '\r') {
        out += static_cast<char>(c);
      }
    } else if (c < 0 || eof_) {
      return out.length() > 0;
    }
    delay(1);
  }
  return out.length() > 0;
}

bool SSHDirectClient::stop(uint32_t timeoutMs) {
  lastStopCloseEagainCount_ = 0;
  lastStopFreeEagainCount_ = 0;
  lastStopDurationMs_ = 0;
  lastStopCloseResult_ = 0;
  lastStopFreeResult_ = 0;

  if (session_ == nullptr || channel_ == nullptr) {
    session_ = nullptr;
    channel_ = nullptr;
    eof_ = true;
    return true;
  }

  if (!session_->isConnected()) {
    session_ = nullptr;
    channel_ = nullptr;
    eof_ = true;
    return true;
  }

  bool closeComplete = false;
  bool freeComplete = false;
  uint32_t start = millis();
  uint32_t deadline = start + timeoutMs;

  while (timeoutMs == 0 || static_cast<int32_t>(millis() - deadline) < 0) {
    if (!session_->lock(pdMS_TO_TICKS(100))) {
      delay(5);
      continue;
    }

    if (!closeComplete) {
      int closeRc = libssh2_channel_close(channel_);
      lastStopCloseResult_ = closeRc;
      if (closeRc == LIBSSH2_ERROR_EAGAIN) {
        lastStopCloseEagainCount_++;
        session_->unlock();
        delay(5);
        continue;
      }
      if (closeRc != 0) {
        lastError_ = closeRc;
      }
      closeComplete = true;
    }

    int freeRc = libssh2_channel_free(channel_);
    lastStopFreeResult_ = freeRc;
    if (freeRc == LIBSSH2_ERROR_EAGAIN) {
      lastStopFreeEagainCount_++;
      session_->unlock();
      delay(5);
      continue;
    }
    if (freeRc != 0) {
      lastError_ = freeRc;
    }
    freeComplete = true;
    session_->unlock();
    break;
  }

  if (!freeComplete) {
    lastError_ = LIBSSH2_ERROR_EAGAIN;
    lastStopDurationMs_ = millis() - start;
    return false;
  }

  session_ = nullptr;
  channel_ = nullptr;
  eof_ = true;
  lastStopDurationMs_ = millis() - start;
  return true;
}
