#include "config.h"
#include <Arduino.h>

// Script state:
//              single thread
//              spi camera - ArduCam Mega (EXCLUDED TEMPORARILY)
//              web page streaming - motion jpeg (EXCLUDED TEMPORARILY)
//              I2C display handler (OLED)
//              I2C password handler
//              I2C display handler (text LCD)
//              websocket
//              wifi reconnect
//              divided into libraries
//              error handling
//              password pprocessing
//              non volatile storage
//              display UI
//              main state machine

#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1

#include <WiFi.h>
#include <time.h>
#include <Wire.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
//#include "SD_MMC.h"
#include <Preferences.h>
#include "websocket.h"
#include "pass_store.h"
#include "box_display.h"
#include "camera.h"
#include "timer.h"
#include "gpio_hal.h"
#include "pass_store.h"
#include "keyboard.h"
#include "state_machine.h"

uint8_t doorStatePin[] = {3, 4, 5, 6};  // Door state input pins
uint8_t doorLockPin[] = {0, 1, 2, 3};   // Door lock control pins

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
bool doorOpen = 0;                              // actual state of door switch (0-closed,1-open)
JsonDocument dispChange;                        // Json Variable to Hold Sensor Readings
unsigned long currentMillis;                    // current time timer
unsigned long startMillis = 0;                  // push button timer
unsigned long switchTime = 0;
//const long interval = 1000;                     // time to open lock DELETE
unsigned long linkStatusMillis = 0;                   // wifi reconnect timer
const long linkStatusInterval = LINK_CHECK_INTERVAL;                 // wifi reconnect delay
keyboardStatus keyboardState;                   // Structure to keep current keyboard info
unsigned long statusLineTimeout = 0;            // Status line refresh control


// Common settings
const int ledPin = 0;
const int doorPin = 3;
const int doorDelay = 50;

// Global instances
GpioHAL gpioHal; // GPIO hardware abstraction layer instance
TimerManager timerManager; // Timer manager instance
WebSocketManager webSocketManager; // Websocket manager instance
PinStorage pinStorage; // Pin storage instance
BoxDisplay boxDisplay; // Box display instance
BoxKeyboard boxKeyboard;
BoxStateMachine boxStateMachine;
//CameraHandler cameraHandler; // Camera handler instance

DoorMapping initialDoorMappings[] = INITIAL_DOOR_MAPPING;

//  void processPassword();         ### asi smazat
//void boxDisplay.logPrint(String logText);
void handleDueActions(uint8_t action, const char* arg);
void onStateChanged(BoxState oldState, BoxState newState, unsigned long currentMillis);

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
      img { 
            max-width: 100%; 
            border: 3px solid #333;
            border-radius: 8px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.3);
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background: white;
            padding: 20px;
            border-radius: 10px;
        }
        .info {
            margin-top: 20px;
            color: #666;
            font-size: 14px;
        }
    </style>
</head>
<body>
  <div class="topnav">
    <h1>ORANGE Box</h1>
    <P>%SCRIPTVER%</P>
  </div>
  <div class="content">
    <div class="card">
      <h2>Locker 0 (GPIO 0)</h2>
      Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec vel sapien eget nunc luctus commodo. Sed at ligula quis sapien bibendum efficitur.
    </div>

 <script>
//  var gateway;
 </script>
</body>
</html>
)rawliteral";

void cameraStart(){

}

void cameraStop(){

}


// Websocket get_ambient request handler
// Reads ambient ligth pin state and returns it in JSON response. Request is expected to be empty, but can contain some parameters in the future, e.g. for calibration of the sensor.
void handleGetAmbient(AsyncWebSocketClient *sender, JsonObj data)
{
    JsonDocument response;
    if (gpioHal.getAmbient() == AMBIENT_ON) {
        response["AMBIENT_OFF_BEACON"] = STATE_NEGATIVE;
        response["AMBIENT_ON_BEACON"] = STATE_POSITIVE;  
    } else {
        response["AMBIENT_OFF_BEACON"] = STATE_POSITIVE;
        response["AMBIENT_ON_BEACON"] = STATE_NEGATIVE;  
    }
    webSocketManager.sendMessage(sender, COMM_VISIBILITY, response);
}

//Get doors state request:
void handleGetDoors(AsyncWebSocketClient *sender, JsonObj data)
{
    JsonDocument response; 
    uint8_t doorNum;
    unsigned long lnum = 0;
    JsonVariantConst doorNumValue = data[MSG_DOORNUM];
    
    if (doorNumValue.isNull()) {
        boxDisplay.logPrint("Error: Missing door number in request");
        response[MSG_LASTRESULT] = "Error: Missing door number in request";
        webSocketManager.sendMessage(sender, COMM_CONTENT, response);
        return;
    }

    if (doorNumValue.is<const char*>()) {
      const char* doorNumStr = doorNumValue.as<const char*>();
      char* endptr;
      if (doorNumStr == nullptr || doorNumStr[0] == '\0') {
        boxDisplay.logPrint("Error: Missing door number in request");
        response[MSG_LASTRESULT] = "Error: Missing door number in request";
        webSocketManager.sendMessage(sender, COMM_CONTENT, response);
        return;
      }
      lnum = strtoul(doorNumStr, &endptr, 10);
      if (*endptr != '\0') {
        boxDisplay.logPrint("Error: Invalid door number " + String(doorNumStr));
        response[MSG_LASTRESULT] = "Error: Invalid door number " + String(doorNumStr);
        webSocketManager.sendMessage(sender, COMM_CONTENT, response);
        return;
      }
    } else if (doorNumValue.is<unsigned long>()) {
      lnum = doorNumValue.as<unsigned long>();
    } else {
      boxDisplay.logPrint("Error: Invalid door number format");
      response[MSG_LASTRESULT] = "Error: Invalid door number format";
      webSocketManager.sendMessage(sender, COMM_CONTENT, response);
      return;
    }

    if (lnum > 254 ) { // 0 is not a valid door number, and 255 is reserved for special purposes
      boxDisplay.logPrint("Error: Invalid door number " + String(lnum));
      response[MSG_LASTRESULT] = "Error: Invalid door number " + String(lnum);
      webSocketManager.sendMessage(sender, COMM_CONTENT, response);
      return;
    }
    doorNum = static_cast<uint8_t>(lnum);
    Serial.println("handleGetDoors called for door number: " + String(doorNum)); //###

//    if (doorNum < DOOR_COUNT) {
    uint8_t isOpen = gpioHal.readDoorState(doorNum);
    if(isOpen != DOOR_UNKNOWN) {
      if (isOpen == DOOR_MIXED) {
        isOpen = DOOR_OPEN;                 // ### to be fixed when UI can properly handle MIXED state, meanwhile show mixed state as open for the response, but log it as a warning
        boxDisplay.logPrint("Warning: Mixed state detected for door " + String(doorNum));
      } 
      if (isOpen == DOOR_OPEN) {
        response[DOOR_OPEN_BEACON] = STATE_POSITIVE;
        response[DOOR_CLOSED_BEACON] = STATE_NEGATIVE;
        response[DOOR_MIXED_BEACON] = STATE_NEGATIVE;
        boxDisplay.logPrint("Door " + String(doorNum) + " is OPEN");
      } else if (isOpen == DOOR_CLOSED) {
        response[DOOR_OPEN_BEACON] = STATE_NEGATIVE;
        response[DOOR_CLOSED_BEACON] = STATE_POSITIVE;
        response[DOOR_MIXED_BEACON] = STATE_NEGATIVE;
        boxDisplay.logPrint("Door " + String(doorNum) + " is CLOSED");
      } else { // isOpen == DOOR_MIXED
        response[DOOR_OPEN_BEACON] = STATE_NEGATIVE;
        response[DOOR_CLOSED_BEACON] = STATE_NEGATIVE;
        response[DOOR_MIXED_BEACON] = STATE_POSITIVE;
        boxDisplay.logPrint("Door " + String(doorNum) + " is in MIXED state");
      }
      webSocketManager.sendMessage(sender, COMM_VISIBILITY, response);
    } else {
      boxDisplay.logPrint("Error: Invalid door number " + String(doorNum));
    }
}

// Websocket get_pins request handler
// Reads pins from storage and returns them in JSON response. Request is expected to contain firstPin and pinCount parameters to specify which pins to return, e.g. for pagination in the UI.
void handleGetPins(AsyncWebSocketClient *sender, JsonObj data)
{
  int8_t firstPinId = data["pinId"];
  int8_t pinCount = data["pinCount"];
  int8_t lastPinId = firstPinId + pinCount - 1;
  JsonDocument response;
  String pinsTable;
  
  size_t pinInfoSize = pinStorage.getPins(firstPinId, pinCount, pinsTable);
  response["pinrows"] = pinsTable;
  webSocketManager.sendMessage(sender, COMM_CONTENT, response);
}

// Websocket get_pins request handler
// Reads pins from storage and returns them in JSON response. Request is expected to contain firstPin and pinCount parameters to specify which pins to return, e.g. for pagination in the UI.
void handleGetPager(AsyncWebSocketClient *sender, JsonObj data)
{
  int8_t firstPinId = data["pinId"];
  int8_t pinCount = data["pinCount"];
  int8_t lastPinId = firstPinId + pinCount - 1;
  
  JsonDocument response;
  String pinsTable;
//  response["pinrows"] = pinStorage.getPins(firstPinId, pinCount, pinsTable);
//  webSocketManager.sendMessage(sender, COMM_CONTENT, response);
// ### sem bude pot5eba napsat obsluhu stránkování, až to GUI bude umět, zatím jen log a prázdný response
  boxDisplay.logPrint("handleGetPager called with firstPinId: " + String(firstPinId) + ", pinCount: " + String(pinCount) + ", lastPinId: " + String(lastPinId) + "\n"); //###
  webSocketManager.sendMessage(sender, COMM_CONTENT, response);
}


void handleOpenBox(AsyncWebSocketClient *sender, JsonObj data)
{
  const char* doorNumStr = data["doornum"];

  if (doorNumStr) {
    JsonDocument response; 
    char* endptr;
    unsigned long lnum = strtoul(doorNumStr, &endptr, 10);
    if (lnum > 254 || *endptr != '\0') { // 0 is not a valid door number, and 255 is reserved for special purposes
      boxDisplay.logPrint("Error: Invalid door number " + String(lnum));
      response["lastresult"] = "Error: Invalid door number " + String(lnum);
      webSocketManager.sendMessage(sender, COMM_CONTENT, response);
      return;
    }
    uint8_t num = static_cast<uint8_t>(lnum);

    BoxEventData event = {};
    event.eventType = BoxEventType::WebOpenBox;
    event.data.doorData.doorNum = num;
    event.data.doorData.doorState = 0;
    boxStateMachine.processEvent(event);
  }
}

void handleSetPin(AsyncWebSocketClient *sender, JsonObj data)
{
  const char* pinCommand;
  pinCommand =  data["clicked"];
	uint32_t pinId = 0;
  bool dataChange = false;
  JsonDocument statusResponse;

  if ( data["pinid"] != nullptr ) {
		pinId = data["pinid"];
  } else {
    pinId = 0; // default value for new pin
  }
  
  if ( strcmp(pinCommand, PIN_DELETE_BEACON) == 0 ) {                   // delete pin
		CacheRecord recToDelete;
    recToDelete.pinId = data[MSG_PINID];
    if (data[MSG_PINNAME] != nullptr) {
      pinId = data[MSG_PINID];
      recToDelete.name[0] = '\0';
      strncat(recToDelete.name, data[MSG_PINNAME], PIN_NAME_LEN); 
      recToDelete.pin[0] = '\0';
      if (data[MSG_PINVALUE] != nullptr) {
        strncat(recToDelete.pin, data[MSG_PINVALUE], PIN_CODE_LEN);
      } else {
        boxDisplay.logPrint("Error deleting pin with id " + String(pinId) + ": pin not provided");
        statusResponse[MSG_LASTRESULT] = "Error deleting pin with id " + String(pinId) + ": pin not provided";
        webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
        return;
      }
      if ( pinStorage.removePin(recToDelete) == pinId ) {
        boxDisplay.logPrint("Pin " + String(pinId) + " / " + String(recToDelete.name) + " deleted successfully");
        statusResponse[MSG_LASTRESULT] = "Pin " + String(pinId) + " / " + String(recToDelete.name) + " deleted successfully";
        webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
        dataChange = true;
      } else {
        boxDisplay.logPrint("Error deleting pin " + String(pinId) + " / " + String(recToDelete.name));
        statusResponse[MSG_LASTRESULT] = "Error deleting pin " + String(pinId) + " / " + String(recToDelete.name);
        webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
      }
    } else {
      boxDisplay.logPrint("Error deleting pin with id " + String(pinId) + ": pin name not provided");
      statusResponse[MSG_LASTRESULT] = "Error deleting pin with id " + String(pinId) + ": pin name not provided";
      webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
    }
  } else if ( strcmp(pinCommand, PIN_SAVE_BEACON) == 0 ) {                    // save or update pin
    CacheRecord recToSave;
    recToSave.pinId = pinId;
    recToSave.name[0] = '\0';
    recToSave.pin[0] = '\0';
    if (data[MSG_PINNAME] != nullptr) {
      strncat(recToSave.name, data[MSG_PINNAME], PIN_NAME_LEN);
    } else {
      boxDisplay.logPrint("Error saving pin with id " + String(pinId) + ": pin name not provided");
      statusResponse[MSG_LASTRESULT] = "Error saving pin with id " + String(pinId) + ": pin name not provided";
      webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
      return;
    }
    if (data[MSG_PINVALUE] != nullptr) {
      strncat(recToSave.pin, data[MSG_PINVALUE], PIN_CODE_LEN);
    } else {
      boxDisplay.logPrint("Error saving pin with id " + String(pinId) + ": pin not provided");
      statusResponse[MSG_LASTRESULT] = "Error saving pin with id " + String(pinId) + ": pin not provided";
      webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
      return;
    }
    if (data[MSG_PINVALIDFROM] != nullptr) {
      recToSave.validFrom = data[MSG_PINVALIDFROM];
    } else {
      recToSave.validFrom = DATE_FROM_UNLIMITED; // default value if not provided
    }
    if (data[MSG_PINVALIDTO] != nullptr) {
      recToSave.validTo = data[MSG_PINVALIDTO];
    } else {
      recToSave.validTo = DATE_TO_UNLIMITED; // default value if not provided
    }
    if (data[MSG_REMAINING] != nullptr) {
      recToSave.remaining = data[MSG_REMAINING];
    } else {
      recToSave.remaining = -1; // defaults to unlimited (-1), 0 means expired
    }
    if (data[MSG_DOORNUM] != nullptr) {
      char* endptr;
      unsigned long lnum = strtoul(data[MSG_DOORNUM], &endptr, 10);
      if (lnum > 254 || lnum == 0 || *endptr != '\0') { // 0 is not a valid door number, and 255 is reserved for special purposes
        boxDisplay.logPrint("Error: Invalid door number " + String(lnum));
        statusResponse[MSG_LASTRESULT] = "Error: Invalid door number " + String(lnum);
        webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
        return;
      }
      recToSave.doorNum = static_cast<uint8_t>(lnum);
    } else {
      recToSave.doorNum = 1; // UI zatim neposkytuje možnost nastavit číslo dveří, takže defaultně nastavíme na 1, ale v budoucnu by to mělo být součástí UI a pak se to bude brát z dat ###
    }
    Serial.printf("Saving pin: id=%u, name=%s, pin=%s, doorNum=%u, validFrom=%llu, validTo=%llu, remaining=%d\n", recToSave.pinId, recToSave.name, recToSave.pin, recToSave.doorNum, recToSave.validFrom, recToSave.validTo, recToSave.remaining); //###
    if (pinId){                   // update existing pin
      if ( pinStorage.updatePin(recToSave) ) {
        boxDisplay.logPrint("Pin " + String(pinId) + " / " + String(recToSave.name) + " saved successfully\n");
        statusResponse[MSG_LASTRESULT] = "Pin '" + String(pinId) + " / " + String(recToSave.name) + "' saved successfully";
        webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
        dataChange = true;
      } else {
        boxDisplay.logPrint("Error saving pin " + String(pinId) + " / " + String(recToSave.name) + "\n");
        statusResponse[MSG_LASTRESULT] = "Error saving pin '" + String(pinId) + " / " + String(recToSave.name) + "'";

        webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
      } 
    } else {                      // add new pin
      uint32_t newPinId = pinStorage.addPin(recToSave);  
      if ( newPinId ) {
        boxDisplay.logPrint("Pin " + String(newPinId) + " / " + String(recToSave.name) + " added successfully\n");
        statusResponse[MSG_LASTRESULT] = "Pin '" + String(newPinId) + " / " + String(recToSave.name) + "' added successfully";
        webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
        dataChange = true;
      } else {
        boxDisplay.logPrint("Error adding new pin " + String(recToSave.name) + "\n");
        statusResponse[MSG_LASTRESULT] = "Error adding new pin '" + String(recToSave.name) + "'";
        webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
      }
    }
  }
  if (dataChange) {
    JsonDocument tableUpdate;
    String pinsTable;
    size_t pinInfoSize = pinStorage.getPins(1 , 100, pinsTable);    //$$$$$$$$$ hardcoded for now, bude potreba implementovat mechanismus notifikace zmeny pintable pro UI - kazdy klient si stahne tu stranku, kterou zrovna zobrazuje, a ta stranka pak bude posilat WS zpravy s požadavkem na aktualizaci tabulky, např. při otevření dialogu pro správu pinů, nebo po každé změně pinů, aby se zajistilo, že UI bude mít aktuální data, ale zároveň se nebude přetěžovat WS zprávami s aktualizací tabulky pro všechny klienty pokaždé, když dojde ke změně pinů, což by mohlo být časté a nemusí být relevantní pro všechny klienty najednou, takže je lepší nechat to na jednotlivých klientech, aby si řekli o aktualizaci tabulky, když ji potřebují)
    tableUpdate["pinrows"] = pinsTable;
    webSocketManager.notifyClients(COMM_CONTENT, tableUpdate);
    Serial.println("Pin table update sent to clients, Update notification still not implemented");
    // data["id"], data["name"], data["pin"], ...
  }
} // handleSetPin

/* ### nejspis k prdu, smazat
// Password processing
void processPassword() {
  if (!pass_ready)
    return;

  Serial.print("Heslo: ");
  for (uint8_t i = 0; i < pass_len; i++)
    Serial.print(pass[i]);
  Serial.println();

    char passStr[15];         // Max IP string length is 15 chars + null terminator
    strcpy ( passStr, "Heslo:" );
    for (uint8_t i = 0; i < pass_len; i++){
      char passNum[4];
      sprintf(passNum, "%d", pass[i]); 
      strcat ( passStr, passNum );
    }
    boxDisplay.logPrint( passStr );
    

  // TODO: validace, akce, atd.

  pass_len = 0;
  pass_ready = false;
} // processPassword
*/

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
}

/****************************/
/****************************/
void setup() {
  Serial.begin(9600);
  Serial.println("\n\n --- B O X   prototype starting! ---\n");

// Initialize I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);  // klasika, žádný spěch

  boxDisplay.displayInit(&gpioHal);
  boxDisplay.logPrint("Display initialized\n");  

  boxDisplay.logPrint("--- BOX prototype ---\n");

  // Initialize timer manager and GPIO HAL (GPIO HAL needs timer manager for scheduling future tasks, e.g. to turn off the lock after some time)
  timerManager.initializeTimerManager(handleDueActions);
  //GpioHAL::ambientPin = AMBIENT_PIN; // Set the ambient light pin in GpioHAL before initializing it
  gpioHal.initializeGpioHAL(&timerManager, initialDoorMappings, AMBIENT_PIN, sizeof(initialDoorMappings) / sizeof(initialDoorMappings[0]));
  boxDisplay.logPrint("-HAL initialized\n");
  // Initializa keyboard (null operation at present)
  boxKeyboard.keyboardInit();
  keyboardState.keyboardMode = KEYBOARD_MODE_COMMAND;
  keyboardState.currentPasswordLen = 0;
  keyboardState.passwordComplete = false;
  keyboardState.cancelPressed = false;
  boxDisplay.logPrint("--Keyboard initialized\n");
  boxStateMachine.initialize();
  boxStateMachine.setStateChangeCallback(onStateChanged);
  boxDisplay.logPrint("---State machine initialized\n");
  // Create mutex for thread safety
  imageMutex = xSemaphoreCreateMutex();

  // Initialize PIN storage
  pinStorage.begin();
  boxDisplay.logPrint ("PIN storage init\n");

  // Initialize camera
  //initCamera();
  //boxDisplay.logPrint("Camera initialized\n");

  // Register callback BEFORE starting preview
//  myCAM.registerCallBack(captureCallback, 200, stopCallback);
//  boxDisplay.logPrint("Camera callback reg\n");

  
  // Connect to WiFi
  WiFi.setHostname(reqhostname);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  boxDisplay.logPrint("Starting WiFi\n");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    boxDisplay.logPrint("WiFi connected!\n");
    IPAddress ip = WiFi.localIP();
    char ipStr[18];         // Max IP string length is 15 chars + null terminator
    sprintf(ipStr, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    boxDisplay.logPrint( ipStr );
    Serial.print(ipStr);
    Serial.print("Open this URL: http://");
    Serial.println(WiFi.localIP());
  } else {
    boxDisplay.logPrint("WiFi FAILED!\n");
  }

 // Configure and start NTP for time synchronization
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
      Serial.println("Current time:");
      Serial.printf("%02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
      Serial.println("Failed to obtain time");
  }

  // Register webserver routes
  registerWebServerRoutes(webserver);

  ElegantOTA.begin(&webserver);

  
  // Start preview mode with 320x240 resolution
  // CAM_VIDEO_MODE_3 = 320x240 according to the enum
  //Serial.println("Starting preview...");
  //myCAM.startPreview(CAM_VIDEO_MODE_3);    // Something strange here: header used by adruino IDE lists many modes, mode 3 means 320x240
  //myCAM.startPreview(CAM_VIDEO_MODE_0);     // header used by Platformio lists only 4 modes, mode 0 means 320x240
  //boxDisplay.logPrint("Streaming active\n");

//Websocket stuff initialization

  webSocketManager.initializeWebSocket(&webserver);
  Serial.println("WebSocket initialized successfully");
  webSocketManager.registerMessageHandler(COMM_SET_PIN,   handleSetPin);
  webSocketManager.registerMessageHandler(COMM_OPEN_BOX,   handleOpenBox);                  // open door
  webSocketManager.registerMessageHandler(COMM_GET_PINS,   handleGetPins);                  // get pin list 
  webSocketManager.registerMessageHandler(COMM_GET_DOOR_STATE,   handleGetDoors);           // get doors state
  webSocketManager.registerMessageHandler(COMM_GET_AMBIENT, handleGetAmbient);              // get ambient light state 
  webSocketManager.registerMessageHandler(COMM_GET_PAGER, handleGetPager);                  // get pager state

  boxDisplay.logPrint("WebSocket Initialized\n");
  // Start server  
  webserver.begin();
  boxDisplay.logPrint("Webserver started!\n");

  /*
// set I/O pins
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  pinMode(doorPin, INPUT);
  */
  boxDisplay.logPrint("SETUP COMPLETE!\n");

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
}

void handleDueActions(uint8_t action, const char* arg) {
  // Implement the logic to perform actions based on the action code and argument
  // For example:
  switch (action) {
    case NOTIFY_DOOR_CHANGE: {
        JsonDocument response;
        uint8_t doorNum = atoi(arg);
        uint8_t isOpen = gpioHal.readDoorState(doorNum);
        if (isOpen == DOOR_MIXED) {
          boxDisplay.logPrint("Warning: Mixed state detected for door " + String(doorNum) + " during notification");
        }
        if (isOpen == DOOR_UNKNOWN) {
          boxDisplay.logPrint("Error: Invalid door number " + String(doorNum) + " in notification");
        }
        if (isOpen == DOOR_OPEN) {
          response["door_state_open"] = "yes";
          response["door_state_closed"] = "no";
          response["door_state_mixed"] = "no";
        } else if (isOpen == DOOR_CLOSED) {
          response["door_state_open"] = "no";
          response["door_state_closed"] = "yes";
          response["door_state_mixed"] = "no";
        } else if (isOpen == DOOR_MIXED) {
          response["door_state_open"] = "no";
          response["door_state_closed"] = "no";
          response["door_state_mixed"] = "yes";
        } 
        webSocketManager.notifyClients(COMM_VISIBILITY, response);      

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
        boxDisplay.logPrint(arg);
        uint8_t lockNum = atoi(arg);
        uint8_t result = gpioHal.lockDeactivate(lockNum);
        if (result != 0) {
          boxDisplay.logPrint("Lock " + String(lockNum) + " deactivated\n");
        } else {
          boxDisplay.logPrint("Error: Invalid lock number in timer callback: " + String(lockNum));
        }      
      }
      break;

    case AMBIENT_DEACTIVATE: 
      boxDisplay.logPrint("Deactivating ambient light\n");
      gpioHal.ambientOff();
      break;
    
    case PASSWORD_TIMEOUT:
        boxDisplay.logPrint("Password timeout passed\n");
        {
          BoxEventData event = {};
          event.eventType = BoxEventType::PasswordTimeout;
          boxStateMachine.processEvent(event);
        }
      break;

    default:
      boxDisplay.logPrint ("Unknown due action called");
    }
     
    // Add more actions as needed
} // handleDueAction

// Check link status - try to reconnect if necessary
void checkLinkStatus() {
  if (WiFi.status() != WL_CONNECTED ) {
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    if (WiFi.status() != WL_CONNECTED ) {
      if (wifiState) {
        Serial.println("WiFi lost");
      }
      wifiState = 0;        
      boxDisplay.setLinkStatus(ONLINE_STATUS_OFFLINE);
    } else {
      if (!wifiState) {
        Serial.println("WiFi restored");
      }
      wifiState = 1;
    }
  } else {
    if (!wifiState) {
      Serial.println("WiFi restored");
      wifiState = 1;
    }
  }
    linkStatusMillis = currentMillis + linkStatusInterval;
} // checkWifi()

void loop() {
  currentMillis = millis();

  timerManager.update(currentMillis);
  boxKeyboard.handleKeyboard(&keyboardState, &boxStateMachine);
  boxStateMachine.update(currentMillis);

  webSocketManager.update(currentMillis);
  ElegantOTA.loop();
  if (currentMillis > linkStatusMillis) {
    checkLinkStatus();
  }

  if ( statusLineTimeout < currentMillis ) {
    statusLineTimeout = currentMillis + STATUSLINE_REFRESH_INTERVAL;
    boxDisplay.writeStatusLine();
  }
// captureThread() processes incoming data and calls our callback
//  myCAM.captureThread();

} // loop
