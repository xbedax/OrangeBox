#include "config.h"
#include "timeout.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Script state:
//              single thread
//              spi camera - ArduCam Mega (EXCLUDED TEMPORARILY)
//              web page streaming - motion jpeg (EXCLUDED TEMPORARILY)
//              I2C debug display handler (OLED)
//              I2C keyboard handler
//              I2C display handler (text LCD)
//              websocket
//              wifi reconnect
//              divided into libraries
//              error handling
//              password pprocessing
//              non volatile storage
//              display UI
//              main state machine
//              logger
//              OTA update
//              reverse SSH tunnel VPN
//              NTP time sync
//              RTC
//              internal diagnostics

#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1

#include <WiFi.h>
#include <time.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
//#include "SD_MMC.h"
#include <Preferences.h>
#include "websocket.h"
#include "pass_store.h"
#include "cred_protocol.h"
#include "box_display.h"
#include "camera.h"
#include "timer.h"
#include "gpio_hal.h"
#include "pass_store.h"
#include "keyboard.h"
#include "state_machine.h"
#include "box_scanner.h"
#include "logger.h"
#include "websocket_log_transport.h"
#include "vpn_manager.h"
#include "box_rtc.h"
#include "diagnostics.h"

//uint8_t doorStatePin[] = {3, 4, 5, 6};  // Door state input pins   - smazat
//uint8_t doorLockPin[] = {0, 1, 2, 3};   // Door lock control pins  - smazat

const char* fversion = "Bx 0.06";

// WiFi credentials
const char* reqhostname = BOX_HOST_NAME;
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWD;

// Camera resolution
#define MAX_IMAGE_SIZE 30000

//I2C settings
#define I2C_ADDR 0x42

// NTP settings
const char* ntpServer = NTP_SERVER;
const long  gmtOffset_sec = GMT_OFFSET_SEC;  // Adjust according to timezone
const int   daylightOffset_sec = DAYLIGHT_OFFSET_SEC;


//  Overal state variables
bool wifiState = 0;                             // actual state of WiFi connection
bool ledState = 0;                              // actual state of door lock (0-closed,1-enganged)
//bool doorOpen = 0;                              // actual state of door switch (0-closed,1-open)
JsonDocument dispChange;                        // Json Variable to Hold Sensor Readings
unsigned long currentMillis;                    // current time timer
// AsyncTCP submits requests; only loop() changes the state machine and GPIO.
static QueueHandle_t webOpenRequests = nullptr;
//unsigned long startMillis = 0;                  // push button timer ###VYHODIT
//unsigned long switchTime = 0;                   // ###VYHODIT

unsigned long linkStatusMillis = 0;             // Last WiFi status check
bool linkStatusStarted = false;
const long linkStatusInterval = LINK_CHECK_INTERVAL;                 // wifi reconnect delay
unsigned long wifiRecoveryResetMillis = 0;
bool wifiRecoveryActive = false;
keyboardStatus keyboardState;                   // Structure to keep current keyboard info
unsigned long statusLineStartedAt = 0;         // Status line refresh control
bool statusLineStarted = false;
char vpnLocalHost[16] = "127.0.0.1";

struct BoxStartInfo {
  time_t startTime = 0;               // wall-clock time when a valid time source first became available
  const char* timeSource = "unknown"; // "RTC" or "NTP", whichever provided startTime first
  unsigned long currentMillis = 0;    // uptime (millis()) at the moment startTime was captured
};
BoxStartInfo lastStart;

// Global instances
GpioHAL gpioHal; // GPIO hardware abstraction layer instance
TimerManager timerManager; // Timer manager instance
WebSocketManager webSocketManager; // Websocket manager instance
WebSocketLogTransport webSocketLogTransport; // Remote logger transport instance
CredStorage credStorage; // Credential storage instance
BoxDisplay boxDisplay; // Box display instance
BoxKeyboard boxKeyboard;
BoxStateMachine boxStateMachine;
BoxScanner boxScanner(boxStateMachine);
HardwareSerial scannerUart(QR_SCANNER_UART_NUM);
BoxRtc boxRtc;
//CameraHandler cameraHandler; // Camera handler instance
DoorMapping initialDoorMappings[] = INITIAL_DOOR_MAPPING;

void handleDueActions(uint8_t action, const char* arg);
void onStateChanged(BoxState oldState, BoxState newState, unsigned long currentMillis);
void refreshVpnLocalHost();
bool startVpn();
String buildDiagnosticsPage();

// Records the first wall-clock time obtained after boot, and where it came from; no-op once recorded.
void recordStartTime(const char* source) {
  if (lastStart.startTime != 0) {
    return;
  }
  time_t nowEpoch = time(nullptr);
  if (nowEpoch < static_cast<time_t>(RTC_MIN_VALID_UNIX_TIME)) {
    return;
  }
  lastStart.startTime = nowEpoch;
  lastStart.timeSource = source;
  lastStart.currentMillis = millis();
}

// Password handling variables
uint8_t pass[PASS_MAX];
uint8_t pass_len = 0;
bool pass_ready = false;

// Web server on port 80
AsyncWebServer webserver(80);

// HTML page with live stream
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
b<html>
<head>
    <title>ORANGE Box</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      html {font-family: serif; display: inline-block; text-align: center;}
      h1 {font-size: 1.8rem; color: white;}
      h2 {font-size: 1.5rem;}
      p {font-size: 1.0rem;}
      .topnav {overflow: hidden; background-color: #143642;}
      body {max-width: 600px; margin:0px auto; padding-bottom: 25px;}
      .content {display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; padding: 20px;}      
      .card { background-color: #F8F7F9;; box-shadow: 2px 2px 12px 1px rgba(100,100,110,.5); padding-top:10px; padding-bottom:20px; }
    </style>
</head>
<body>
  <div class="topnav">
    <h1>ORANGE Box</h1>
    <P>Nothing to see here</P>
  </div>
  <div class="content">
    <div class="card">
      Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec vel sapien eget nunc luctus commodo. Sed at ligula quis sapien bibendum efficitur.
    </div>

</body>
</html>
)rawliteral";

void cameraStart(){

}

void cameraStop(){

}


// Separate human-readable status from structured replies and diagnostic content.
void sendLastResult(AsyncWebSocketClient* recipient, JsonDocument& response) {
    if (!response[MSG_LASTRESULT].is<const char*>()) return;
    JsonDocument status;
    status[MSG_LASTRESULT] = response[MSG_LASTRESULT];
    response.remove(MSG_LASTRESULT);
    webSocketManager.sendMessage(recipient, COMM_LASTRESULT, status);
}

// State payloads are shared by direct replies and change notifications.
void sendDoorState(AsyncWebSocketClient* recipient, uint8_t doorNum, uint8_t state) {
    if (state != DOOR_OPEN && state != DOOR_CLOSED && state != DOOR_MIXED) return;
    JsonDocument response;
    response["door_num"] = doorNum;
    // The UI has two states: a partly open group must not appear closed.
    response["state"] = state == DOOR_CLOSED ? "closed" : "open";
    if (recipient) webSocketManager.sendMessage(recipient, COMM_DOOR_STATE, response);
    else webSocketManager.notifyClients(COMM_DOOR_STATE, response);
}

void sendAmbientState(AsyncWebSocketClient* recipient, uint8_t state) {
    JsonDocument response;
    response["state"] = state == AMBIENT_ON ? "on" : "off";
    if (recipient) webSocketManager.sendMessage(recipient, COMM_AMBIENT_STATE, response);
    else webSocketManager.notifyClients(COMM_AMBIENT_STATE, response);
}

void notifyAmbientChange() {
    static uint8_t lastState = INVALID_PIN;
    const uint8_t state = gpioHal.getAmbient();
    if (state != lastState) {
        sendAmbientState(nullptr, state);
        lastState = state;
    }
}

// Websocket get_ambient request handler.
void handleGetAmbient(AsyncWebSocketClient *sender, JsonObj data)
{
    (void)data;
    sendAmbientState(sender, gpioHal.getAmbient());
}

//Get doors state request:
void handleGetDoors(AsyncWebSocketClient *sender, JsonObj data)
{
    JsonDocument response; 
    uint8_t doorNum;
    unsigned long lnum = 0;
    JsonVariantConst doorNumValue = data[MSG_DOORNUM];
    
    if (doorNumValue.isNull()) {
        logger.logPrint(SEVERITY_ERROR, "Error: Missing door number in request", BOX_HOST_NAME, LOGAREA_ACCESS);
        response[MSG_LASTRESULT] = "Error: Missing door number in request";
        webSocketManager.sendMessage(sender, COMM_LASTRESULT, response);
        return;
    }

    if (doorNumValue.is<const char*>()) {
      const char* doorNumStr = doorNumValue.as<const char*>();
      char* endptr;
      if (doorNumStr == nullptr || doorNumStr[0] == '\0') {
        logger.logPrint(SEVERITY_ERROR, "Error: Missing door number in request", BOX_HOST_NAME, LOGAREA_ACCESS);
        response[MSG_LASTRESULT] = "Error: Missing door number in request";
        webSocketManager.sendMessage(sender, COMM_LASTRESULT, response);
        return;
      }
      lnum = strtoul(doorNumStr, &endptr, 10);
      if (*endptr != '\0') {
        logger.logPrint(SEVERITY_ERROR, "Error: Invalid door number " + String(doorNumStr), BOX_HOST_NAME, LOGAREA_ACCESS);
        response[MSG_LASTRESULT] = "Error: Invalid door number " + String(doorNumStr);
        webSocketManager.sendMessage(sender, COMM_LASTRESULT, response);
        return;
      }
    } else if (doorNumValue.is<unsigned long>()) {
      lnum = doorNumValue.as<unsigned long>();
    } else {
      logger.logPrint(SEVERITY_ERROR, "Error: Invalid door number format", BOX_HOST_NAME, LOGAREA_ACCESS);
      response[MSG_LASTRESULT] = "Error: Invalid door number format";
      webSocketManager.sendMessage(sender, COMM_LASTRESULT, response);
      return;
    }

    if (lnum > 254 ) { // Door 0 is valid; 255 is reserved for unknown/special values
      logger.logPrint(SEVERITY_ERROR, "Error: Invalid door number " + String(lnum), BOX_HOST_NAME, LOGAREA_ACCESS);
      response[MSG_LASTRESULT] = "Error: Invalid door number " + String(lnum);
      webSocketManager.sendMessage(sender, COMM_LASTRESULT, response);
      return;
    }
    doorNum = static_cast<uint8_t>(lnum);
    Serial.println("handleGetDoors called for door number: " + String(doorNum)); //###

//    if (doorNum < DOOR_COUNT) {
    uint8_t isOpen = gpioHal.readDoorState(doorNum);
    if(isOpen != DOOR_UNKNOWN) {
      if (isOpen == DOOR_MIXED) {
        logger.logPrint(SEVERITY_WARNING, "Warning: Mixed state detected for door " + String(doorNum), BOX_HOST_NAME, LOGAREA_ACCESS);
      }
      sendDoorState(sender, doorNum, isOpen);
    } else {
      logger.logPrint(SEVERITY_ERROR, "Error: Invalid door number " + String(doorNum), BOX_HOST_NAME, LOGAREA_ACCESS);
    }
}

// Credential handlers share parsing/formatting with the native protocol tests.
void handleGetCreds(AsyncWebSocketClient* sender, JsonObj data)
{
  JsonDocument response;
  buildCredResponse(credStorage, data, response);
  sendLastResult(sender, response);
  webSocketManager.sendMessage(sender, COMM_CREDS, response);
}

void handleGetDiagnostics(AsyncWebSocketClient *sender, JsonObj data)
{
  (void)data;
  JsonDocument response;
  response["html"] = boxDiagnostics.showStatistics(DiagnosticFormat::Html);
  response["text"] = boxDiagnostics.showStatistics(DiagnosticFormat::Text);
  response["json"] = boxDiagnostics.statisticsJson();
  webSocketManager.sendMessage(sender, COMM_DIAGNOSTICS, response);
}

void handleSnapshotDiagnostics(AsyncWebSocketClient *sender, JsonObj data)
{
  (void)data;
  JsonDocument response;
  bool stored = boxDiagnostics.snapshotStatistics();
  response[MSG_LASTRESULT] = stored ? "Diagnostic snapshot stored" : "Diagnostic snapshot failed";
  response["count"] = boxDiagnostics.snapshotCount();
  response["html"] = boxDiagnostics.showSnapshots(DiagnosticFormat::Html);
  sendLastResult(sender, response);
  webSocketManager.sendMessage(sender, COMM_DIAGNOSTIC_SNAPSHOTS, response);
}

void handleGetDiagnosticSnapshots(AsyncWebSocketClient *sender, JsonObj data)
{
  (void)data;
  JsonDocument response;
  response["count"] = boxDiagnostics.snapshotCount();
  response["html"] = boxDiagnostics.showSnapshots(DiagnosticFormat::Html);
  response["text"] = boxDiagnostics.showSnapshots(DiagnosticFormat::Text);
  response["json"] = boxDiagnostics.snapshotsJson();
  webSocketManager.sendMessage(sender, COMM_DIAGNOSTIC_SNAPSHOTS, response);
}


void handleOpenBox(AsyncWebSocketClient *sender, JsonObj data)
{
  const char* doorNumStr = data["doornum"];
  char presenceCode[PRESENCE_CODE_LENGTH + 1] = {0};

  if (doorNumStr) {
    JsonDocument response; 
    char* endptr;
    unsigned long lnum = strtoul(doorNumStr, &endptr, 10);
    if (lnum > 254 || *endptr != '\0') { // 0 is not a valid door number, and 255 is reserved for special purposes
      logger.logPrint(SEVERITY_ERROR, "Error: Invalid door number " + String(lnum), BOX_HOST_NAME, LOGAREA_ACCESS);
      response["lastresult"] = "Error: Invalid door number " + String(lnum);
      webSocketManager.sendMessage(sender, COMM_LASTRESULT, response);
      return;
    }
    if (data[MSG_PRESENCECODE] != nullptr) {
      strncat(presenceCode, data[MSG_PRESENCECODE], PRESENCE_CODE_LENGTH);
    }
    if (data[MSG_CHECKPRESENCE] != nullptr) {
      bool checkPresence = data[MSG_CHECKPRESENCE];
      if (checkPresence && strlen(presenceCode) == 0) {
        logger.logPrint(SEVERITY_ERROR, "Error: Presence code required but not provided", BOX_HOST_NAME, LOGAREA_ACCESS);
        response[MSG_LASTRESULT] = "Error: Presence code required but not provided";
        webSocketManager.sendMessage(sender, COMM_LASTRESULT, response);
        return;
      }
      if(checkPresence && (!boxStateMachine.isPresenceCodeValid() || strcmp(presenceCode, boxStateMachine.getPresenceCode()) != 0)) {
        logger.logPrint(SEVERITY_ERROR, "Error: Invalid presence code " + String(presenceCode), BOX_HOST_NAME, LOGAREA_ACCESS);
        response[MSG_LASTRESULT] = "Error: Invalid presence code " + String(presenceCode);
        webSocketManager.sendMessage(sender, COMM_LASTRESULT, response);
        return;
      }
    }
    uint8_t num = static_cast<uint8_t>(lnum);

    if (!webOpenRequests || xQueueSend(webOpenRequests, &num, 0) != pdTRUE) {
      response[MSG_LASTRESULT] = "Error: Door request queue unavailable or full";
    } else {
      logger.logPrint(SEVERITY_INFO, "Opening box door number " + String(num), BOX_HOST_NAME, LOGAREA_ACCESS);
      response[MSG_LASTRESULT] = "Opening door number " + String(num);
    }
    webSocketManager.sendMessage(sender, COMM_LASTRESULT, response);
  }
}

void handleSetCred(AsyncWebSocketClient* sender, JsonObj data)
{
  JsonDocument response;
  const bool changed = applyCredChange(credStorage, data, response);
  sendLastResult(sender, response);
  webSocketManager.sendMessage(sender, COMM_CREDS, response);
  if (changed) {
    // Each client reloads its current page, preserving its type and pagination.
    JsonDocument notification;
    notification["credType"] = response["credType"];
    notification["changed"] = true;
    webSocketManager.notifyClients(COMM_CREDS, notification);
  }
}

void handleCapture(AsyncWebServerRequest *request) {
  while (xSemaphoreTake(imageMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    delay(5);
  }
  if (writeBuffer != readBuffer) {
    request->send(200, "image/jpeg", (const uint8_t *)imageBuffer[readBuffer], imageLength[readBuffer]);
    xSemaphoreGive(imageMutex);
  } else {
    xSemaphoreGive(imageMutex);
    request->send(503, "text/plain", "Image not ready");
  }
} // handleCapture

String buildDiagnosticsPage() {
  String page;
  page.reserve(2800);
  page += F("<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  page += F("<title>BOX diagnostics</title><style>");
  page += F("body{font-family:Arial,sans-serif;max-width:920px;margin:0 auto;padding:24px;background:#f5f7f8;color:#182024}");
  page += F("h1{font-size:1.8rem;margin:0 0 18px}h2{font-size:1.2rem;margin:24px 0 8px}");
  page += F("table{width:100%;border-collapse:collapse;background:#fff}th,td{text-align:left;padding:8px 10px;border-bottom:1px solid #d9e0e4}");
  page += F("th{background:#143642;color:#fff}code{font-family:Consolas,monospace}p{color:#52616a}");
  page += F("a.button{display:inline-block;margin:0 8px 16px 0;padding:8px 10px;background:#143642;color:#fff;text-decoration:none}");
  page += F("</style></head><body><h1>BOX diagnostics</h1>");
  page += F("<p><strong>Last start:</strong> ");
  if (lastStart.startTime != 0) {
    struct tm startInfo;
    char startBuffer[24];
    if (localtime_r(&lastStart.startTime, &startInfo) != nullptr &&
        strftime(startBuffer, sizeof(startBuffer), "%Y-%m-%d %H:%M:%S", &startInfo) > 0) {
      page += startBuffer;
    } else {
      page += String(static_cast<long>(lastStart.startTime));
    }
    page += F(" (source: ");
    page += lastStart.timeSource;
    page += F(", uptime at capture: ");
    page += String(lastStart.currentMillis);
    page += F(" ms)</p>");
  } else {
    page += F("not yet available</p>");
  }
  page += F("<a class=\"button\" href=\"/diagnostics/snapshot\">Store snapshot</a>");
  page += F("<a class=\"button\" href=\"/diagnostics.txt\">Text</a>");
  page += F("<a class=\"button\" href=\"/diagnostics.json\">JSON</a>");
  page += boxDiagnostics.showStatistics(DiagnosticFormat::Html);
  page += boxDiagnostics.showSnapshots(DiagnosticFormat::Html);
  page += F("</body></html>");
  return page;
}

void registerWebServerRoutes(AsyncWebServer &server) {
  // Stream endpoint - Motion JPEG (simple approach - tries to send complete frame)
  server.on("/stream2", AsyncWebRequestMethod::HTTP_GET, handleStream2);

  // Stream endpoint - Motion JPEG (chunked approach with static state)
  server.on("/stream3", AsyncWebRequestMethod::HTTP_GET, handleStream3);

  // Stream endpoint - Motion JPEG (client-specific state approach)
  server.on("/stream", AsyncWebRequestMethod::HTTP_GET, handleStream);

  // Single capture endpoint (captures one image on demand)
  server.on("/capture", AsyncWebRequestMethod::HTTP_GET, handleCapture);

  server.on("/diagnostics", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", buildDiagnosticsPage());
  });

  server.on("/diagnostics.txt", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    String output = boxDiagnostics.showStatistics(DiagnosticFormat::Text);
    output += '\n';
    output += boxDiagnostics.showSnapshots(DiagnosticFormat::Text);
    request->send(200, "text/plain", output);
  });

  server.on("/diagnostics.json", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", boxDiagnostics.statisticsJson());
  });

  server.on("/diagnostics/snapshot", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    boxDiagnostics.snapshotStatistics();
    request->redirect("/diagnostics");
  });

  server.on("/diagnostics/snapshots.json", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", boxDiagnostics.snapshotsJson());
  });
}

void refreshVpnLocalHost() {
  IPAddress ip = WiFi.localIP();
  snprintf(vpnLocalHost, sizeof(vpnLocalHost), "%u.%u.%u.%u",
           ip[0], ip[1], ip[2], ip[3]);
}

bool startVpn() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  refreshVpnLocalHost();

  VPNConfig vpnConfig;
  vpnConfig.sshHost = VPN_GATEWAY_HOST;
  vpnConfig.sshPort = VPN_GATEWAY_PORT;
  vpnConfig.username = VPN_USERNAME;
  vpnConfig.authMode = VPNAuthMode::PublicKey;
  vpnConfig.privateKey = VPN_PRIVATE_KEY;
  vpnConfig.publicKey = VPN_PUBLIC_KEY;
  vpnConfig.keyPassphrase = "";
  vpnConfig.verifyHostKey = true;
  vpnConfig.hostKeyFingerprint = VPN_GATEWAY_FINGERPRINT;
  vpnConfig.hostKeyType = VPN_GATEWAY_KEY_TYPE;
  vpnConfig.tunnel.remoteBindHost = "127.0.0.1";
  vpnConfig.tunnel.remoteBindPort = VPN_REMOTE_BIND_PORT;
  vpnConfig.tunnel.localHost = vpnLocalHost;
  vpnConfig.tunnel.localPort = 80;
  vpnConfig.keepAliveIntervalSec = VPN_CLIENT_KEEPALIVE_INTERVAL_SEC;
  vpnConfig.reconnectDelayMs = 5000;
  vpnConfig.staleRemoteListenerCooldownMs = VPN_STALE_REMOTE_LISTENER_COOLDOWN_MS;
  vpnConfig.maxReconnectAttempts = 10;
  vpnConfig.connectionTimeoutSec = 30;
  vpnConfig.bufferSize = 8192;
  vpnConfig.maxChannels = 5;
  vpnConfig.debugEnabled = false;

  if (!vpnManager.begin(vpnConfig)) {
    logger.logPrint(SEVERITY_ERROR, "VPN begin failed: " + vpnManager.getStateString(), BOX_HOST_NAME, LOGAREA_COMM);
    return false;
  }

  logger.logPrint(SEVERITY_INFO,
                  "VPN configured: 127.0.0.1:" + String(VPN_REMOTE_BIND_PORT) +
                  " -> " + String(vpnLocalHost) + ":80",
                  BOX_HOST_NAME, LOGAREA_COMM);

  logger.logPrint(SEVERITY_INFO, "VPN connection scheduled", BOX_HOST_NAME, LOGAREA_COMM);
  return true;
}

void UIcontrolCallback(uint8_t action ) { /*const char* arg, AsyncWebSocketClient *sender*/
  JsonDocument response;
  if (action == UI_DISABLE_DOOR_CONTROLS) {
    response[COMM_OPEN_BOX] = MSG_DISABLE;
    webSocketManager.notifyClients(COMM_ENORDIS, response);
    return;
  }
  if (action == UI_ENABLE_DOOR_CONTROLS) {
    response[COMM_OPEN_BOX] = MSG_ENABLE;
    webSocketManager.notifyClients(COMM_ENORDIS, response);
    return;
  }
  
}

/****************************/
/****************************/
void setup() {
  webOpenRequests = xQueueCreate(4, sizeof(uint8_t));
  delay(3000); // Small delay to allow any pending operations to complete before starting Serial
  Serial.begin(9600);
  logger.begin(&webSocketLogTransport);
  boxDiagnostics.begin();
  Serial.println("\n\n --- B O X   prototype starting! ---\n");

// Initialize I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);  // klasika, zadny spech

  if (boxRtc.begin(RTC_I2C_ADDRESS, &Wire)) {
    Serial.print("RTC OSF flag: ");
    Serial.println(boxRtc.hasLostPowerFlag() ? "set" : "clear");
    if (boxRtc.setSystemTimeFromRtc()) {
      if (boxRtc.hasLostPowerFlag()) {
        logger.logPrint(SEVERITY_WARNING, "System time loaded from RTC with OSF set; waiting for NTP correction\n", BOX_HOST_NAME, LOGAREA_SYSTEM);
      } else {
        logger.logPrint(SEVERITY_INFO, "System time loaded from RTC\n", BOX_HOST_NAME, LOGAREA_SYSTEM);
        recordStartTime("RTC");
      }
    } else {
      logger.logPrint(SEVERITY_WARNING, "RTC available, but time is not trusted yet\n", BOX_HOST_NAME, LOGAREA_SYSTEM);
    }
  } else {
    logger.logPrint(SEVERITY_WARNING, "RTC not found at 0x" + String(RTC_I2C_ADDRESS, HEX) + "\n", BOX_HOST_NAME, LOGAREA_SYSTEM);
  }

  time_t boxNow = time(nullptr);
  Serial.print("Box current time epoch: ");
  Serial.println(static_cast<long>(boxNow));

  struct tm boxTimeInfo;
  if (localtime_r(&boxNow, &boxTimeInfo) != nullptr) {
    char boxTimeBuffer[24];
    if (strftime(boxTimeBuffer, sizeof(boxTimeBuffer), "%Y-%m-%d %H:%M:%S", &boxTimeInfo) > 0) {
      Serial.print("Box current time: ");
      Serial.println(boxTimeBuffer);
    }
  }

  boxDisplay.displayInit(&gpioHal);
  logger.logPrint(SEVERITY_INFO, "Display initialized\n", BOX_HOST_NAME, LOGAREA_SYSTEM);

  logger.logPrint(SEVERITY_INFO, "--- BOX prototype ---\n", BOX_HOST_NAME, LOGAREA_SYSTEM);

  // Initialize timer manager and GPIO HAL (GPIO HAL needs timer manager for scheduling future tasks, e.g. to turn off the lock after some time)
  timerManager.initializeTimerManager(handleDueActions);
  //GpioHAL::ambientPin = AMBIENT_PIN; // Set the ambient light pin in GpioHAL before initializing it
  gpioHal.initializeGpioHAL(&timerManager, initialDoorMappings, AMBIENT_PIN, sizeof(initialDoorMappings) / sizeof(initialDoorMappings[0]));
  logger.logPrint(SEVERITY_INFO, "-HAL initialized\n", BOX_HOST_NAME, LOGAREA_SYSTEM);
  // Initializa keyboard (null operation at present)
  boxKeyboard.keyboardInit();
  keyboardState.keyboardMode = KEYBOARD_MODE_COMMAND;
  keyboardState.currentPasswordLen = 0;
  keyboardState.passwordComplete = false;
  keyboardState.cancelPressed = false;
  logger.logPrint(SEVERITY_INFO, "--Keyboard initialized\n", BOX_HOST_NAME, LOGAREA_SYSTEM);
  scannerUart.begin(QR_SCANNER_BAUD, SERIAL_8N1, QR_SCANNER_RX_PIN, QR_SCANNER_TX_PIN);
  if (!boxScanner.begin(scannerUart)) {
    logger.logPrint(SEVERITY_ERROR, "Scanner command configuration is invalid", BOX_HOST_NAME, LOGAREA_SYSTEM);
  }
  boxStateMachine.initialize();
  boxStateMachine.setStateChangeCallback(onStateChanged);
  boxStateMachine.setUIUpdateCallback(UIcontrolCallback);
  logger.logPrint(SEVERITY_INFO, "---State machine initialized\n", BOX_HOST_NAME, LOGAREA_SYSTEM);
  // Create mutex for thread safety
  imageMutex = xSemaphoreCreateMutex();

  // Initialize camera
  //initCamera();
  //logger.logPrint(SEVERITY_INFO, "Camera initialized\n", BOX_HOST_NAME, LOGAREA_SYSTEM);

  // Register callback BEFORE starting preview
//  myCAM.registerCallBack(captureCallback, 200, stopCallback);
//  logger.logPrint(SEVERITY_INFO, "Camera callback reg\n", BOX_HOST_NAME, LOGAREA_SYSTEM);

  
  // Connect to WiFi
  WiFi.setHostname(reqhostname);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  logger.logPrint(SEVERITY_INFO, "Starting WiFi\n", BOX_HOST_NAME, LOGAREA_COMM);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    logger.logPrint(SEVERITY_INFO, "WiFi connected!\n", BOX_HOST_NAME, LOGAREA_COMM);
    wifiState = 1;
    boxDisplay.setLinkStatus(ONLINE_STATUS_WIFI);
    IPAddress ip = WiFi.localIP();
    char ipStr[18];         // Max IP string length is 15 chars + null terminator
    sprintf(ipStr, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    logger.logPrint(SEVERITY_INFO, ipStr, BOX_HOST_NAME, LOGAREA_COMM);
    Serial.print(ipStr);
    Serial.print("Open this URL: http://");
    Serial.println(WiFi.localIP());
    boxDisplay.setCommunicationStatus(COMMUNICATION_STATUS_OK);
  } else {
    wifiState = 0;
    boxDisplay.setLinkStatus(ONLINE_STATUS_OFFLINE);
    logger.logPrint(SEVERITY_ERROR, "WiFi FAILED!\n", BOX_HOST_NAME, LOGAREA_COMM);
  }

  if (boxRtc.configureNtp(ntpServer, gmtOffset_sec, daylightOffset_sec, RTC_NTP_SYNC_INTERVAL_MS)) {
    logger.logPrint(SEVERITY_INFO, "NTP synchronization scheduled\n", BOX_HOST_NAME, LOGAREA_SYSTEM);
  } else {
    logger.logPrint(SEVERITY_WARNING, "NTP synchronization not configured\n", BOX_HOST_NAME, LOGAREA_SYSTEM);
  }


  // Initialize credential storage after the RTC load/NTP setup, so date-limited
  // records are evaluated against the best available time.
  credStorage.begin();
  logger.logPrint(SEVERITY_INFO, "Credential storage init\n", BOX_HOST_NAME, LOGAREA_SYSTEM);

  // Register webserver routes
  registerWebServerRoutes(webserver);

  ElegantOTA.begin(&webserver);

  
  // Start preview mode with 320x240 resolution
  // CAM_VIDEO_MODE_3 = 320x240 according to the enum
  //Serial.println("Starting preview...");
  //myCAM.startPreview(CAM_VIDEO_MODE_3);    // Something strange here: header used by adruino IDE lists many modes, mode 3 means 320x240
  //myCAM.startPreview(CAM_VIDEO_MODE_0);     // header used by Platformio lists only 4 modes, mode 0 means 320x240
  //logger.logPrint(SEVERITY_INFO, "Streaming active\n", BOX_HOST_NAME, LOGAREA_SYSTEM);

//Websocket stuff initialization

  webSocketManager.initializeWebSocket(&webserver);
  Serial.println("WebSocket initialized successfully");
  webSocketManager.registerMessageHandler(COMM_SET_CRED,   handleSetCred);
  webSocketManager.registerMessageHandler(COMM_OPEN_BOX,   handleOpenBox);                  // open door
  webSocketManager.registerMessageHandler(COMM_GET_CREDS,   handleGetCreds);                  // get credential list
  webSocketManager.registerMessageHandler(COMM_GET_DOOR_STATE,   handleGetDoors);           // get doors state
  webSocketManager.registerMessageHandler(COMM_GET_AMBIENT, handleGetAmbient);              // get ambient light state 
  webSocketManager.registerMessageHandler(COMM_GET_DIAGNOSTICS, handleGetDiagnostics);
  webSocketManager.registerMessageHandler(COMM_SNAPSHOT_DIAGNOSTICS, handleSnapshotDiagnostics);
  webSocketManager.registerMessageHandler(COMM_GET_DIAGNOSTIC_SNAPSHOTS, handleGetDiagnosticSnapshots);

  logger.logPrint(SEVERITY_INFO, "WebSocket Initialized\n", BOX_HOST_NAME, LOGAREA_COMM);
  // Start server  
  webserver.begin();
  logger.logPrint(SEVERITY_INFO, "Webserver started!\n", BOX_HOST_NAME, LOGAREA_COMM);
  startVpn();

  /*
// set I/O pins
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  pinMode(doorPin, INPUT);
  */
  logger.logPrint(SEVERITY_INFO, "SETUP COMPLETE!\n", BOX_HOST_NAME, LOGAREA_SYSTEM);
  boxDiagnostics.snapshotStatistics();

}  //setup

void onStateChanged(BoxState oldState, BoxState newState, unsigned long currentMillis) {
  (void)oldState;
  (void)currentMillis;

  if (newState == BoxState::Open) {
    cameraStart();
  }

  if (newState == BoxState::Home) {
    cameraStop();
  }

  if (newState == BoxState::Password) {
    keyboardState.keyboardMode = KEYBOARD_MODE_PASSWORD;
    keyboardState.currentPasswordLen = 0;
    keyboardState.passwordComplete = false;
    keyboardState.cancelPressed = false;
    boxKeyboard.clearPassword(&keyboardState);
  } else {
    keyboardState.keyboardMode = KEYBOARD_MODE_COMMAND;
  }
  // Last: a failed start can synchronously return the state machine to Home.
  boxScanner.onStateChanged(oldState, newState);
}

void handleDueActions(uint8_t action, const char* arg) {
  // Implement the logic to perform actions based on the action code and argument
  // For example:
  switch (action) {
    case NOTIFY_DOOR_CHANGE: {
        uint8_t doorNum = atoi(arg);
        uint8_t isOpen = gpioHal.readDoorState(doorNum);
        if (isOpen == DOOR_MIXED) {
          logger.logPrint(SEVERITY_WARNING, "Warning: Mixed state detected for door " + String(doorNum) + " during notification", BOX_HOST_NAME, LOGAREA_ACCESS);
        }
        if (isOpen == DOOR_UNKNOWN) {
          logger.logPrint(SEVERITY_ERROR, "Error: Invalid door number " + String(doorNum) + " in notification", BOX_HOST_NAME, LOGAREA_ACCESS);
        }
        sendDoorState(nullptr, doorNum, isOpen);

        if (isOpen == DOOR_OPEN || isOpen == DOOR_CLOSED) {
          BoxEventData event = {};
          event.eventType = (isOpen == DOOR_OPEN) ? BoxEventType::DoorOpened : BoxEventType::DoorClosed;
          event.data.doorData.doorNum = doorNum;
          event.data.doorData.doorState = isOpen;
          boxStateMachine.processEvent(event);
        }
      }
      break;
    case LOCK_DEACTIVATE: {
        logger.logPrint(SEVERITY_DEBUG, arg, BOX_HOST_NAME, LOGAREA_SYSTEM);
        uint8_t lockNum = atoi(arg);
        uint8_t result = gpioHal.lockDeactivate(lockNum);
        if (result != DOOR_UNKNOWN) {
          logger.logPrint(SEVERITY_INFO, "Lock " + String(lockNum) + " deactivated\n", BOX_HOST_NAME, LOGAREA_ACCESS);
        } else {
          logger.logPrint(SEVERITY_ERROR, "Error: Invalid lock number in timer callback: " + String(lockNum), BOX_HOST_NAME, LOGAREA_ACCESS);
        }      
      }
      break;

    case AMBIENT_DEACTIVATE: 
      logger.logPrint(SEVERITY_INFO, "Deactivating ambient light\n", BOX_HOST_NAME, LOGAREA_SYSTEM);
      gpioHal.ambientOff();
      break;
    
    case PASSWORD_TIMEOUT:
        logger.logPrint(SEVERITY_INFO, "Password timeout passed\n", BOX_HOST_NAME, LOGAREA_ACCESS);
        {
          BoxEventData event = {};
          event.eventType = BoxEventType::PasswordTimeout;
          boxStateMachine.processEvent(event);
        }
      break;

    default:
      logger.logPrint(SEVERITY_ERROR, "Unknown due action called", BOX_HOST_NAME, LOGAREA_SYSTEM);
    }
     
    // Add more actions as needed
} // handleDueAction

// Check link status - try to reconnect if necessary
void checkLinkStatus() {
  if (WiFi.status() != WL_CONNECTED ) {
    if (wifiState) {
      Serial.println("WiFi lost");
      vpnManager.disconnect();
      wifiRecoveryResetMillis = currentMillis;
      wifiRecoveryActive = true;
    }

    if (!wifiRecoveryActive) {
      wifiRecoveryResetMillis = currentMillis;
      wifiRecoveryActive = true;
      WiFi.reconnect();
    } else if (timeoutElapsed(wifiRecoveryResetMillis, WIFI_RECOVERY_RESET_INTERVAL_MS, currentMillis)) {
      // Auto reconnect can remain stuck after an AP disappears. Restart the
      // association only after allowing the previous attempt to complete.
      Serial.println("Restarting WiFi association");
      WiFi.disconnect(false, false);
      WiFi.begin(ssid, password);
      wifiRecoveryResetMillis = currentMillis;
      wifiRecoveryActive = true;
    }
    wifiState = 0;
    boxDisplay.setLinkStatus(ONLINE_STATUS_OFFLINE);
  } else {
    if (!wifiState) {
      Serial.println("WiFi restored");
      wifiState = 1;
      startVpn();
    }
    wifiRecoveryActive = false;
    boxDisplay.setLinkStatus(ONLINE_STATUS_WIFI);
  }
    linkStatusMillis = currentMillis;
    linkStatusStarted = true;
} // checkWifi()

void loop() {
  currentMillis = millis();

  if (boxRtc.update(currentMillis)) {
    credStorage.refreshCache();
    recordStartTime("NTP");
  }
  uint8_t requestedDoor;
  if (webOpenRequests && xQueueReceive(webOpenRequests, &requestedDoor, 0) == pdTRUE) {
    BoxEventData event = {};
    event.eventType = BoxEventType::WebOpenBox;
    event.data.doorData.doorNum = requestedDoor;
    boxStateMachine.processEvent(event);
  }
  gpioHal.updateDoorStates(millis(), [](uint8_t door) {
    char arg[4];
    snprintf(arg, sizeof(arg), "%u", door);
    handleDueActions(NOTIFY_DOOR_CHANGE, arg);
  });
  timerManager.update(millis());
  boxKeyboard.handleKeyboard(&keyboardState, &boxStateMachine);
  // Keyboard events can start a timeout after the loop timestamp was sampled.
  currentMillis = millis();
  boxStateMachine.update(currentMillis);
  boxScanner.update(millis());
  // Observe actual GPIO state after timers and state transitions have run.
  notifyAmbientChange();
  logger.update(currentMillis);

  webSocketManager.update(currentMillis);
  vpnManager.update();
  ElegantOTA.loop();
  if (!linkStatusStarted || timeoutElapsed(linkStatusMillis, linkStatusInterval, currentMillis)) {
    checkLinkStatus();
  }

  if ( !statusLineStarted || timeoutElapsed(statusLineStartedAt, STATUSLINE_REFRESH_INTERVAL, currentMillis) ) {
    statusLineStartedAt = currentMillis;
    statusLineStarted = true;
    uint8_t clientCount = webSocketManager.getClientCount();
    boxDisplay.setCommunicationStatus(clientCount <= 9 ? char(clientCount + '0') : '+');
    boxDisplay.setLoggerStatus(webSocketLogTransport.hasPendingOutput() ? LOGGER_STATUS_DISCONNECTED : LOGGER_STATUS_CONNECTED);
    boxDisplay.writeStatusLine();
  }
// captureThread() processes incoming data and calls our callback
//  myCAM.captureThread();

} // loop
