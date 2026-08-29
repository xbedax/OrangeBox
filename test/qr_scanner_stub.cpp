#include <Arduino.h>
#include <HardwareSerial.h>
#include <cstring>

#include "config.h"
#include "qr_scanner.h"

static QrScanner qrScanner;

static const char configuredActivateCommand[] = QR_SCANNER_ACTIVATE_COMMAND;
static const char configuredDeactivateCommand[] = QR_SCANNER_DEACTIVATE_COMMAND;

static char consoleLine[192];
static size_t consoleLineLen = 0;

static HardwareSerial& scannerSerial()
{
#if QR_SCANNER_UART_NUM == 0
#if ARDUINO_USB_CDC_ON_BOOT
  return Serial0;
#else
  return Serial;
#endif
#elif QR_SCANNER_UART_NUM == 1
  return Serial1;
#elif QR_SCANNER_UART_NUM == 2
  return Serial2;
#else
  static HardwareSerial fallbackScannerSerial(QR_SCANNER_UART_NUM);
  return fallbackScannerSerial;
#endif
}

static void skipSpaces(const char*& text)
{
  while (*text == ' ' || *text == '\t') {
    text++;
  }
}

static bool startsCommand(const char* line, const char* command)
{
  while (*command != '\0') {
    if (*line++ != *command++) {
      return false;
    }
  }
  return *line == '\0' || *line == ' ' || *line == '\t';
}

static int hexValue(char ch)
{
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  }
  if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  return -1;
}

static bool parseHexPayload(const char* input, uint8_t* output, size_t outputSize, size_t& outputLen)
{
  outputLen = 0;
  bool haveHighNibble = false;
  uint8_t highNibble = 0;

  for (const char* p = input; *p != '\0'; p++) {
    if (*p == ' ' || *p == '\t' || *p == ',' || *p == ':' || *p == '-') {
      continue;
    }

    int value = hexValue(*p);
    if (value < 0) {
      return false;
    }

    if (!haveHighNibble) {
      highNibble = static_cast<uint8_t>(value);
      haveHighNibble = true;
      continue;
    }

    if (outputLen >= outputSize) {
      return false;
    }
    output[outputLen++] = static_cast<uint8_t>((highNibble << 4) | value);
    haveHighNibble = false;
  }

  return !haveHighNibble && outputLen > 0;
}

static bool parseEscapedPayload(const char* input, uint8_t* output, size_t outputSize, size_t& outputLen)
{
  outputLen = 0;

  for (const char* p = input; *p != '\0'; p++) {
    uint8_t value = static_cast<uint8_t>(*p);

    if (*p == '\\' && p[1] != '\0') {
      p++;
      switch (*p) {
        case 'r':
          value = '\r';
          break;
        case 'n':
          value = '\n';
          break;
        case 't':
          value = '\t';
          break;
        case '0':
          value = 0;
          break;
        case '\\':
          value = '\\';
          break;
        case 'x':
          {
            int high = hexValue(p[1]);
            int low = hexValue(p[2]);
            if (high < 0 || low < 0) {
              return false;
            }
            value = static_cast<uint8_t>((high << 4) | low);
            p += 2;
          }
          break;
        default:
          value = static_cast<uint8_t>(*p);
          break;
      }
    }

    if (outputLen >= outputSize) {
      return false;
    }
    output[outputLen++] = value;
  }

  return outputLen > 0;
}

static bool parsePayload(const char* input, uint8_t* output, size_t outputSize, size_t& outputLen)
{
  skipSpaces(input);
  if (input[0] == 'h' && input[1] == 'e' && input[2] == 'x'
      && (input[3] == ':' || input[3] == ' ' || input[3] == '\t')) {
    input += 3;
    skipSpaces(input);
    if (*input == ':') {
      input++;
    }
    skipSpaces(input);
    return parseHexPayload(input, output, outputSize, outputLen);
  }

  return parseEscapedPayload(input, output, outputSize, outputLen);
}

static bool parseOptionalTimeout(const char* input, unsigned long& timeoutMs)
{
  skipSpaces(input);
  if (*input == '\0') {
    return true;
  }

  char* endPtr = nullptr;
  unsigned long parsed = strtoul(input, &endPtr, 10);
  if (endPtr == input) {
    return false;
  }

  while (*endPtr == ' ' || *endPtr == '\t') {
    endPtr++;
  }
  if (*endPtr != '\0') {
    return false;
  }

  timeoutMs = parsed;
  return true;
}

static void printBytes(const uint8_t* data, size_t length)
{
  Serial.print("text=\"");
  for (size_t i = 0; i < length; i++) {
    uint8_t value = data[i];
    if (value == '\r') {
      Serial.print("\\r");
    } else if (value == '\n') {
      Serial.print("\\n");
    } else if (value == '\t') {
      Serial.print("\\t");
    } else if (value == '\\') {
      Serial.print("\\\\");
    } else if (value >= 32 && value <= 126) {
      Serial.print(static_cast<char>(value));
    } else {
      Serial.print("\\x");
      if (value < 16) {
        Serial.print('0');
      }
      Serial.print(value, HEX);
    }
  }
  Serial.print("\" hex=");
  for (size_t i = 0; i < length; i++) {
    if (data[i] < 16) {
      Serial.print('0');
    }
    Serial.print(data[i], HEX);
    if (i + 1 < length) {
      Serial.print(' ');
    }
  }
}

static void printFrameLine(const char* prefix, const uint8_t* data, size_t length)
{
  Serial.print(prefix);
  Serial.print(" len=");
  Serial.print(static_cast<unsigned>(length));
  Serial.print(' ');
  printBytes(data, length);
  Serial.println();
}

static const char* scannerStateName(QrScannerState state)
{
  switch (state) {
    case QrScannerState::Idle:
      return "Idle";
    case QrScannerState::WaitingActivationResponse:
      return "WaitingActivationResponse";
    case QrScannerState::WaitingCode:
      return "WaitingCode";
    case QrScannerState::WaitingCommandResponse:
      return "WaitingCommandResponse";
    default:
      return "?";
  }
}

static void printStatus()
{
  Serial.print("[qr] state=");
  Serial.print(scannerStateName(qrScanner.state()));
  Serial.print(" active=");
  Serial.println(qrScanner.isActive() ? "yes" : "no");
}

static void printConfig()
{
  Serial.println();
  Serial.println("QR scanner stub configuration:");
  Serial.print("  console baud: ");
  Serial.println(QR_SCANNER_STUB_CONSOLE_BAUD);
  Serial.print("  scanner uart: ");
  Serial.print(QR_SCANNER_UART_NUM);
  Serial.print(" baud=");
  Serial.print(QR_SCANNER_BAUD);
  Serial.print(" rx=");
  Serial.print(QR_SCANNER_RX_PIN);
  Serial.print(" tx=");
  Serial.println(QR_SCANNER_TX_PIN);
  Serial.print("  switch pin: ");
  Serial.print(QR_SCANNER_SWITCH_PIN);
  Serial.print(" activeHigh=");
  Serial.println(QR_SCANNER_SWITCH_ACTIVE_HIGH ? "yes" : "no");
  Serial.print("  response window ms: ");
  Serial.println(QR_SCANNER_RESPONSE_WINDOW_MS);
  Serial.print("  command response window ms: ");
  Serial.println(QR_SCANNER_COMMAND_RESPONSE_WINDOW_MS);
  Serial.print("  scan timeout ms: ");
  Serial.println(QR_SCANNER_SCAN_TIMEOUT_MS);

  if (sizeof(configuredActivateCommand) > 1) {
    printFrameLine("  activate command:", reinterpret_cast<const uint8_t*>(configuredActivateCommand), sizeof(configuredActivateCommand) - 1);
  } else {
    Serial.println("  activate command: <empty>");
  }

  if (sizeof(configuredDeactivateCommand) > 1) {
    printFrameLine("  deactivate command:", reinterpret_cast<const uint8_t*>(configuredDeactivateCommand), sizeof(configuredDeactivateCommand) - 1);
  } else {
    Serial.println("  deactivate command: <empty>");
  }
  Serial.println();
}

static void printHelp()
{
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  help or ?             show this help");
  Serial.println("  status                show scanner state");
  Serial.println("  config                show UART/pin/command configuration");
  Serial.println("  start [timeout_ms]    activate with configured command/switch and wait for code");
  Serial.println("  act <payload>         activate with custom payload and wait for code");
  Serial.println("  cmd <payload>         send payload and capture command response window");
  Serial.println("  raw <payload>         send payload without changing scanner state");
  Serial.println("  stop [payload]        deactivate, optionally with custom payload");
  Serial.println();
  Serial.println("Payloads:");
  Serial.println("  text with escapes:    \\r \\n \\t \\\\ \\0 \\xHH");
  Serial.println("  hex bytes:            hex: 16 54 0D");
  Serial.println();
}

static void configureScanner()
{
  QrScannerConfig scannerConfig;
  scannerConfig.activateCommand.bytes = reinterpret_cast<const uint8_t*>(configuredActivateCommand);
  scannerConfig.activateCommand.length = sizeof(configuredActivateCommand) - 1;
  scannerConfig.deactivateCommand.bytes = reinterpret_cast<const uint8_t*>(configuredDeactivateCommand);
  scannerConfig.deactivateCommand.length = sizeof(configuredDeactivateCommand) - 1;
  scannerConfig.switchPin = QR_SCANNER_SWITCH_PIN;
  scannerConfig.switchActiveHigh = QR_SCANNER_SWITCH_ACTIVE_HIGH != 0;
  scannerConfig.activationResponseWindowMs = QR_SCANNER_RESPONSE_WINDOW_MS;
  scannerConfig.commandResponseWindowMs = QR_SCANNER_COMMAND_RESPONSE_WINDOW_MS;
  scannerConfig.frameIdleMs = QR_SCANNER_FRAME_IDLE_MS;
  scannerConfig.defaultScanTimeoutMs = QR_SCANNER_SCAN_TIMEOUT_MS;

  bool hasCommand = scannerConfig.activateCommand.length > 0;
  bool hasSwitch = scannerConfig.switchPin != QR_SCANNER_PIN_UNUSED;
  if (hasCommand && hasSwitch) {
    scannerConfig.activationMode = QrScannerActivationMode::CommandAndSwitch;
  } else if (hasCommand) {
    scannerConfig.activationMode = QrScannerActivationMode::Command;
  } else if (hasSwitch) {
    scannerConfig.activationMode = QrScannerActivationMode::SwitchPin;
  } else {
    scannerConfig.activationMode = QrScannerActivationMode::Manual;
  }

  qrScanner.begin(scannerSerial(), scannerConfig);
}

static void handleConsoleLine(char* line)
{
  const char* input = line;
  skipSpaces(input);
  if (*input == '\0') {
    return;
  }
  Serial.println("Input: " + String(input));

  if (strcmp(input, "?") == 0 || startsCommand(input, "help")) {
    printHelp();
    return;
  }

  if (startsCommand(input, "status")) {
    printStatus();
    return;
  }

  if (startsCommand(input, "config")) {
    printConfig();
    return;
  }

  if (startsCommand(input, "start")) {
    input += 5;
    unsigned long timeoutMs = QR_SCANNER_SCAN_TIMEOUT_MS;
    if (!parseOptionalTimeout(input, timeoutMs)) {
      Serial.println("[cmd] invalid timeout");
      return;
    }
    if (qrScanner.startScan(timeoutMs)) {
      Serial.println("[cmd] scan started");
    } else {
      Serial.println("[cmd] scan start failed");
    }
    return;
  }

  if (startsCommand(input, "act")) {
    input += 3;
    uint8_t payload[QR_SCANNER_MAX_FRAME_LEN];
    size_t payloadLen = 0;
    if (!parsePayload(input, payload, sizeof(payload), payloadLen)) {
      Serial.println("[cmd] invalid activation payload");
      return;
    }
    if (qrScanner.startScan(payload, payloadLen, QR_SCANNER_SCAN_TIMEOUT_MS)) {
      printFrameLine("[cmd] activation sent:", payload, payloadLen);
    } else {
      Serial.println("[cmd] activation failed");
    }
    return;
  }

  if (startsCommand(input, "cmd")) {
    input += 3;
    uint8_t payload[QR_SCANNER_MAX_FRAME_LEN];
    size_t payloadLen = 0;
    if (!parsePayload(input, payload, sizeof(payload), payloadLen)) {
      Serial.println("[cmd] invalid command payload");
      return;
    }
    if (qrScanner.sendCommand(payload, payloadLen, QR_SCANNER_COMMAND_RESPONSE_WINDOW_MS)) {
      printFrameLine("[cmd] command sent:", payload, payloadLen);
    } else {
      Serial.println("[cmd] command send failed");
    }
    return;
  }

  if (startsCommand(input, "raw")) {
    input += 3;
    uint8_t payload[QR_SCANNER_MAX_FRAME_LEN];
    size_t payloadLen = 0;
    if (!parsePayload(input, payload, sizeof(payload), payloadLen)) {
      Serial.println("[cmd] invalid raw payload");
      return;
    }
    scannerSerial().write(payload, payloadLen);
    printFrameLine("[cmd] raw sent:", payload, payloadLen);
    return;
  }

  if (startsCommand(input, "stop")) {
    input += 4;
    skipSpaces(input);
    if (*input == '\0') {
      qrScanner.deactivate();
      Serial.println("[cmd] scanner deactivated");
      return;
    }

    uint8_t payload[QR_SCANNER_MAX_FRAME_LEN];
    size_t payloadLen = 0;
    if (!parsePayload(input, payload, sizeof(payload), payloadLen)) {
      Serial.println("[cmd] invalid deactivate payload");
      return;
    }
    qrScanner.deactivate(payload, payloadLen);
    printFrameLine("[cmd] deactivate sent:", payload, payloadLen);
    return;
  }

  Serial.println("[cmd] unknown command, type help");
}

static void readConsole()
{
  while (Serial.available() > 0) {
    char ch = static_cast<char>(Serial.read());
    if (ch == '\r' || ch == '\n') {
      consoleLine[consoleLineLen] = '\0';
      handleConsoleLine(consoleLine);
      consoleLineLen = 0;
      continue;
    }

    if (consoleLineLen + 1 < sizeof(consoleLine)) {
      consoleLine[consoleLineLen++] = ch;
    } else {
      consoleLineLen = 0;
      Serial.println("[cmd] input line too long");
    }
  }
}

static void printScannerEvent(const QrScannerEvent& event)
{
  switch (event.type) {
    case QrScannerEventType::None:
      break;
    case QrScannerEventType::Activated:
      Serial.println("[qr] activated");
      break;
    case QrScannerEventType::ActivationResponse:
      printFrameLine("[qr] activation response:", event.data, event.length);
      break;
    case QrScannerEventType::CommandResponse:
      printFrameLine("[qr] command response:", event.data, event.length);
      break;
    case QrScannerEventType::CodeReceived:
      printFrameLine("[qr] code:", event.data, event.length);
      Serial.println("[qr] scanner deactivated after code");
      break;
    case QrScannerEventType::ScanTimeout:
      Serial.println("[qr] scan timeout, scanner deactivated");
      break;
    case QrScannerEventType::CommandResponseTimeout:
      Serial.println("[qr] command response window closed");
      break;
    case QrScannerEventType::Deactivated:
      Serial.println("[qr] deactivated");
      break;
    case QrScannerEventType::RxOverflow:
      Serial.println("[qr] receive frame overflow");
      break;
    case QrScannerEventType::UnexpectedFrame:
      printFrameLine("[qr] unexpected frame:", event.data, event.length);
      break;
    default:
      Serial.println("[qr] unknown event");
      break;
  }
}

void setup()
{
  Serial.begin(QR_SCANNER_STUB_CONSOLE_BAUD);
#if ARDUINO_USB_CDC_ON_BOOT
  unsigned long serialWaitUntil = millis() + 3000;
  while (!Serial && static_cast<long>(millis() - serialWaitUntil) < 0) {
    delay(10);
  }
#endif
  delay(3000);
  Serial.println();
  Serial.println("QR scanner UART stub");

  scannerSerial().begin(QR_SCANNER_BAUD, SERIAL_8N1, QR_SCANNER_RX_PIN, QR_SCANNER_TX_PIN);
  configureScanner();
  printConfig();
  printHelp();
}

void loop()
{
  readConsole();
  printScannerEvent(qrScanner.update());
  delay(1);
}
