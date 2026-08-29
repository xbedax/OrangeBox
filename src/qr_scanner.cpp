#include "qr_scanner.h"

QrScanner::QrScanner()
{
}

void QrScanner::begin(Stream& serialStream, const QrScannerConfig& scannerConfig)
{
  uart = &serialStream;
  config = scannerConfig;
  currentState = QrScannerState::Idle;
  scanActive = false;
  responseDeadline = 0;
  scanDeadline = 0;
  pendingEvent = QrScannerEventType::None;
  resetReceiver();

  if (config.switchPin != QR_SCANNER_PIN_UNUSED) {
    pinMode(config.switchPin, OUTPUT);
    setSwitchActive(false);
  }
}

bool QrScanner::startScan(unsigned long timeoutMs)
{
  return startScan(nullptr, 0, timeoutMs);
}

bool QrScanner::startScan(const char* activateCommandOverride, unsigned long timeoutMs)
{
  if (activateCommandOverride == nullptr) {
    return startScan(nullptr, 0, timeoutMs);
  }
  return startScan(reinterpret_cast<const uint8_t*>(activateCommandOverride),
                   std::strlen(activateCommandOverride),
                   timeoutMs);
}

bool QrScanner::startScan(const uint8_t* activateCommandOverride, size_t commandLength, unsigned long timeoutMs)
{
  if (uart == nullptr) {
    return false;
  }

  if (timeoutMs == 0) {
    timeoutMs = config.defaultScanTimeoutMs;
  }

  if (scanActive) {
    deactivateAfterTerminalEvent();
  }

  resetReceiver();
  activateHardware(activateCommandOverride, commandLength);

  scanActive = true;
  unsigned long now = millis();
  scanDeadline = timeoutMs == 0 ? 0 : now + timeoutMs;
  responseDeadline = config.activationResponseWindowMs == 0
                       ? now
                       : now + config.activationResponseWindowMs;
  currentState = config.activationResponseWindowMs == 0
                   ? QrScannerState::WaitingCode
                   : QrScannerState::WaitingActivationResponse;
  pendingEvent = QrScannerEventType::Activated;
  return true;
}

bool QrScanner::sendCommand(const char* command, unsigned long responseWindowMs)
{
  if (command == nullptr) {
    return false;
  }
  return sendCommand(reinterpret_cast<const uint8_t*>(command),
                     std::strlen(command),
                     responseWindowMs);
}

bool QrScanner::sendCommand(const uint8_t* command, size_t commandLength, unsigned long responseWindowMs)
{
  if (uart == nullptr || command == nullptr || commandLength == 0) {
    return false;
  }

  resetReceiver();
  writeCommand(command, commandLength);

  unsigned long now = millis();
  if (responseWindowMs == 0) {
    responseWindowMs = config.commandResponseWindowMs;
  }
  responseDeadline = responseWindowMs == 0 ? now : now + responseWindowMs;
  currentState = responseWindowMs == 0 ? QrScannerState::Idle : QrScannerState::WaitingCommandResponse;
  return true;
}

void QrScanner::deactivate()
{
  deactivate(nullptr, 0);
}

void QrScanner::deactivate(const char* deactivateCommandOverride)
{
  if (deactivateCommandOverride == nullptr) {
    deactivate(nullptr, 0);
    return;
  }
  deactivate(reinterpret_cast<const uint8_t*>(deactivateCommandOverride),
             std::strlen(deactivateCommandOverride));
}

void QrScanner::deactivate(const uint8_t* deactivateCommandOverride, size_t commandLength)
{
  deactivateHardware(deactivateCommandOverride, commandLength);
  currentState = QrScannerState::Idle;
  scanActive = false;
  scanDeadline = 0;
  responseDeadline = 0;
  pendingEvent = QrScannerEventType::Deactivated;
}

QrScannerEvent QrScanner::update()
{
  return update(millis());
}

QrScannerEvent QrScanner::update(unsigned long now)
{
  QrScannerEvent pending = popPendingEvent();
  if (pending.type != QrScannerEventType::None) {
    return pending;
  }

  if (uart == nullptr) {
    return makeEvent(QrScannerEventType::None);
  }

  while (uart->available() > 0) {
    int value = uart->read();
    if (value < 0) {
      break;
    }

    QrScannerEvent event = processByte(static_cast<uint8_t>(value), now);
    if (event.type != QrScannerEventType::None) {
      return event;
    }
  }

  if (rxFrameLen > 0 && config.frameIdleMs > 0
      && static_cast<long>(now - lastByteMillis) >= static_cast<long>(config.frameIdleMs)) {
    return finalizeFrame();
  }

  if (currentState == QrScannerState::WaitingActivationResponse
      && static_cast<long>(now - responseDeadline) >= 0) {
    currentState = QrScannerState::WaitingCode;
  }

  if (currentState == QrScannerState::WaitingCommandResponse
      && static_cast<long>(now - responseDeadline) >= 0) {
    currentState = scanActive ? QrScannerState::WaitingCode : QrScannerState::Idle;
    return makeEvent(QrScannerEventType::CommandResponseTimeout);
  }

  if (scanActive && scanDeadline != 0
      && static_cast<long>(now - scanDeadline) >= 0) {
    deactivateAfterTerminalEvent();
    return makeEvent(QrScannerEventType::ScanTimeout);
  }

  return makeEvent(QrScannerEventType::None);
}

QrScannerState QrScanner::state() const
{
  return currentState;
}

bool QrScanner::isActive() const
{
  return scanActive;
}

void QrScanner::activateHardware(const uint8_t* commandOverride, size_t commandLength)
{
  if (config.activationMode == QrScannerActivationMode::SwitchPin
      || config.activationMode == QrScannerActivationMode::CommandAndSwitch) {
    setSwitchActive(true);
  }

  if (commandOverride != nullptr && commandLength > 0) {
    writeCommand(commandOverride, commandLength);
    return;
  }

  if (config.activationMode == QrScannerActivationMode::Command
      || config.activationMode == QrScannerActivationMode::CommandAndSwitch) {
    writeCommand(config.activateCommand);
  }
}

void QrScanner::deactivateHardware(const uint8_t* commandOverride, size_t commandLength)
{
  if (commandOverride != nullptr && commandLength > 0) {
    writeCommand(commandOverride, commandLength);
  } else {
    writeCommand(config.deactivateCommand);
  }

  if (config.activationMode == QrScannerActivationMode::SwitchPin
      || config.activationMode == QrScannerActivationMode::CommandAndSwitch) {
    setSwitchActive(false);
  }
}

void QrScanner::writeCommand(const QrScannerCommand& command)
{
  writeCommand(command.bytes, command.length);
}

void QrScanner::writeCommand(const uint8_t* bytes, size_t length)
{
  if (uart == nullptr || bytes == nullptr || length == 0) {
    return;
  }
  uart->write(bytes, length);
}

void QrScanner::setSwitchActive(bool active)
{
  if (config.switchPin == QR_SCANNER_PIN_UNUSED) {
    return;
  }

  bool outputHigh = active ? config.switchActiveHigh : !config.switchActiveHigh;
  digitalWrite(config.switchPin, outputHigh ? HIGH : LOW);
}

void QrScanner::resetReceiver()
{
  rxFrameLen = 0;
  lastByteMillis = 0;
  lastFrameLen = 0;
}

QrScannerEvent QrScanner::processByte(uint8_t value, unsigned long now)
{
  lastByteMillis = now;

  if (isTerminator(value)) {
    if (rxFrameLen == 0) {
      return makeEvent(QrScannerEventType::None);
    }
    return finalizeFrame();
  }

  if (rxFrameLen >= QR_SCANNER_MAX_FRAME_LEN) {
    rxFrameLen = 0;
    return makeEvent(QrScannerEventType::RxOverflow);
  }

  rxFrame[rxFrameLen++] = value;
  return makeEvent(QrScannerEventType::None);
}

QrScannerEvent QrScanner::finalizeFrame()
{
  const uint8_t* data = rxFrame;
  size_t length = rxFrameLen;
  trimFrame(data, length);
  rxFrameLen = 0;

  if (length == 0) {
    return makeEvent(QrScannerEventType::None);
  }

  switch (currentState) {
    case QrScannerState::WaitingActivationResponse:
      return makeFrameEvent(QrScannerEventType::ActivationResponse, data, length);

    case QrScannerState::WaitingCommandResponse:
      return makeFrameEvent(QrScannerEventType::CommandResponse, data, length);

    case QrScannerState::WaitingCode:
      {
        QrScannerEvent event = makeFrameEvent(QrScannerEventType::CodeReceived, data, length);
        deactivateAfterTerminalEvent();
        return event;
      }

    case QrScannerState::Idle:
    default:
      return makeFrameEvent(QrScannerEventType::UnexpectedFrame, data, length);
  }
}

QrScannerEvent QrScanner::makeEvent(QrScannerEventType type)
{
  QrScannerEvent event;
  event.type = type;
  event.data = nullptr;
  event.length = 0;
  return event;
}

QrScannerEvent QrScanner::makeFrameEvent(QrScannerEventType type, const uint8_t* data, size_t length)
{
  if (length > QR_SCANNER_MAX_FRAME_LEN) {
    length = QR_SCANNER_MAX_FRAME_LEN;
  }

  if (length > 0) {
    std::memcpy(lastFrame, data, length);
  }
  lastFrameLen = length;

  QrScannerEvent event;
  event.type = type;
  event.data = lastFrame;
  event.length = lastFrameLen;
  return event;
}

QrScannerEvent QrScanner::popPendingEvent()
{
  if (pendingEvent == QrScannerEventType::None) {
    return makeEvent(QrScannerEventType::None);
  }

  QrScannerEventType eventType = pendingEvent;
  pendingEvent = QrScannerEventType::None;
  return makeEvent(eventType);
}

bool QrScanner::isTerminator(uint8_t value) const
{
  if (config.frameTerminators == nullptr || config.frameTerminators[0] == '\0') {
    return false;
  }

  for (const char* terminator = config.frameTerminators; *terminator != '\0'; terminator++) {
    if (value == static_cast<uint8_t>(*terminator)) {
      return true;
    }
  }
  return false;
}

void QrScanner::trimFrame(const uint8_t*& data, size_t& length) const
{
  if (!config.trimFrames) {
    return;
  }

  while (length > 0 && (data[0] == ' ' || data[0] == '\t')) {
    data++;
    length--;
  }

  while (length > 0 && (data[length - 1] == ' ' || data[length - 1] == '\t')) {
    length--;
  }
}

void QrScanner::deactivateAfterTerminalEvent()
{
  deactivateHardware(nullptr, 0);
  currentState = QrScannerState::Idle;
  scanActive = false;
  scanDeadline = 0;
  responseDeadline = 0;
}
