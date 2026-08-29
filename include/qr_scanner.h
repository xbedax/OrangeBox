#ifndef QR_SCANNER_H
#define QR_SCANNER_H

#include <Arduino.h>
#include <cstring>

#define QR_SCANNER_PIN_UNUSED 255
#define QR_SCANNER_MAX_FRAME_LEN 160

enum class QrScannerActivationMode : uint8_t {
  Manual = 0,
  Command = 1,
  SwitchPin = 2,
  CommandAndSwitch = 3
};

enum class QrScannerState : uint8_t {
  Idle = 0,
  WaitingActivationResponse,
  WaitingCode,
  WaitingCommandResponse
};

enum class QrScannerEventType : uint8_t {
  None = 0,
  Activated,
  ActivationResponse,
  CommandResponse,
  CodeReceived,
  ScanTimeout,
  CommandResponseTimeout,
  Deactivated,
  RxOverflow,
  UnexpectedFrame
};

struct QrScannerCommand {
  const uint8_t* bytes = nullptr;
  size_t length = 0;
};

struct QrScannerConfig {
  QrScannerActivationMode activationMode = QrScannerActivationMode::Command;
  QrScannerCommand activateCommand;
  QrScannerCommand deactivateCommand;
  uint8_t switchPin = QR_SCANNER_PIN_UNUSED;
  bool switchActiveHigh = true;
  const char* frameTerminators = "\r\n";
  unsigned long activationResponseWindowMs = 300;
  unsigned long commandResponseWindowMs = 500;
  unsigned long frameIdleMs = 40;
  unsigned long defaultScanTimeoutMs = 10000;
  bool trimFrames = true;
};

struct QrScannerEvent {
  QrScannerEventType type = QrScannerEventType::None;
  const uint8_t* data = nullptr;
  size_t length = 0;
};

class QrScanner {
public:
  QrScanner();

  void begin(Stream& serialStream, const QrScannerConfig& scannerConfig);

  bool startScan(unsigned long timeoutMs = 0);
  bool startScan(const char* activateCommandOverride, unsigned long timeoutMs = 0);
  bool startScan(const uint8_t* activateCommandOverride, size_t commandLength, unsigned long timeoutMs = 0);

  bool sendCommand(const char* command, unsigned long responseWindowMs = 0);
  bool sendCommand(const uint8_t* command, size_t commandLength, unsigned long responseWindowMs = 0);

  void deactivate();
  void deactivate(const char* deactivateCommandOverride);
  void deactivate(const uint8_t* deactivateCommandOverride, size_t commandLength);

  QrScannerEvent update();
  QrScannerEvent update(unsigned long now);

  QrScannerState state() const;
  bool isActive() const;

private:
  Stream* uart = nullptr;
  QrScannerConfig config;
  QrScannerState currentState = QrScannerState::Idle;
  bool scanActive = false;

  uint8_t rxFrame[QR_SCANNER_MAX_FRAME_LEN];
  size_t rxFrameLen = 0;
  unsigned long lastByteMillis = 0;

  uint8_t lastFrame[QR_SCANNER_MAX_FRAME_LEN];
  size_t lastFrameLen = 0;
  QrScannerEventType pendingEvent = QrScannerEventType::None;

  unsigned long responseDeadline = 0;
  unsigned long scanDeadline = 0;

  void activateHardware(const uint8_t* commandOverride, size_t commandLength);
  void deactivateHardware(const uint8_t* commandOverride, size_t commandLength);
  void writeCommand(const QrScannerCommand& command);
  void writeCommand(const uint8_t* bytes, size_t length);
  void setSwitchActive(bool active);
  void resetReceiver();

  QrScannerEvent processByte(uint8_t value, unsigned long now);
  QrScannerEvent finalizeFrame();
  QrScannerEvent makeEvent(QrScannerEventType type);
  QrScannerEvent makeFrameEvent(QrScannerEventType type, const uint8_t* data, size_t length);
  QrScannerEvent popPendingEvent();

  bool isTerminator(uint8_t value) const;
  void trimFrame(const uint8_t*& data, size_t& length) const;
  void deactivateAfterTerminalEvent();
};

#endif // QR_SCANNER_H
