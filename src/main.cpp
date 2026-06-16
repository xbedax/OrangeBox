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

uint8_t doorStatePin[] = {3, 4, 5, 6};  // Door state input pins
uint8_t doorLockPin[] = {0, 1, 2, 3};   // Door lock control pins

const char* fversion = "Bx 0.07";

// WiFi credentials
const char* reqhostname = BOX_HOST_NAME;
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWD;

// Camera resolution
#define MAX_IMAGE_SIZE 30000

//I2C settings
#define I2C_ADDR 0x42

#define BOXSTATE_HOME           1     //  initial state -> all closed, ambi inactive, no input
#define BOXSTATE_PASSWORD       2     //  keyboard password input
#define BOXSTATE_PRESENCE       3     //  presence code displayed
#define BOXSTATE_OPENING        4     //  valid box open request received (lock pulse running)
#define BOXSTATE_OPEN           5     //  some door opened
#define BOXSTATE_CLOSED         6     //   all door closed, ambi and camera still running
#define BOXSTATE_EXTERNAL       7     //   external command active
#define BOXSTATE_BADPASS        8     //  waiting after bad password entered

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
const long interval = 1000;                     // time to open lock
unsigned long linkStatusMillis = 0;                   // wifi reconnect timer
const long linkStatusInterval = LINK_CHECK_INTERVAL;                 // wifi reconnect delay
keyboardStatus keyboardState;                   // Structure to keep current keyboard info
char presenceCode[PRESENCE_CODE_LENGTH];        // Active presence code
unsigned long presenceCodeExpiration;           // When presence code expires
uint8_t  badPasswordCount = 0;                  // Number of consecutive bad password entry
unsigned long badPasswordDelayFinish = 0;       //  End of actual bad password delay
unsigned long currentBadPasswordMillis = 0;     // Actual bad password delay
uint8_t actualState = BOXSTATE_HOME;            // Master state of the box 
unsigned long displayActionMillis = 0;          // Used to plan refresh of action line content
uint8_t doorToOpen = 0;                         //  Currently opened door - at most one in any situation
unsigned long doorOpenTimeout = 0;              // Waiting for door openning limitation
unsigned long ambientOffTimeout = 0;            // Dealyed ambient switching off
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
//CameraHandler cameraHandler; // Camera handler instance

void handleKey(uint8_t k);
//  void processPassword();         ### asi smazat
//void boxDisplay.logPrint(String logText);
void handleDueActions(uint8_t action, const char* arg);

// Password handling variables
uint8_t pass[PASS_MAX];
uint8_t pass_len = 0;
bool pass_ready = false;

// Web server on port 80
AsyncWebServer webserver(80);
//AsyncWebSocket wsserver("/ws");

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

void enableExternal(){
//je potreba doplnit do UI zakaz tlacitka pro otevreni boxu a indikaci, ze box nekdo obsluhuje prezencne
//dodelat prislusne zpravy
// funkce bude povolovat externi ovladani
}

void disableExternal(){
//je potreba doplnit do UI zakaz tlacitka pro otevreni boxu a indikaci, ze box nekdo obsluhuje prezencne
//dodelat prislusne zpravy
// funkce bude zakazovat externi ovladani
}

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
    uint8_t doorNum = data["doornum"];
    if (doorNum < DOOR_COUNT) {
      uint8_t isOpen = gpioHal.readDoorState(doorNum);
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
  response["pinrows"] = pinStorage.getPins(firstPinId, pinCount, pinsTable);
  webSocketManager.sendMessage(sender, COMM_CONTENT, response);
}

// ### z tohoto vyhodit zpracovani seznamu dveri k otevreni - neni potreba, je reseno logickym cislem
void handleOpenBox(AsyncWebSocketClient *sender, JsonObj data)
{
  const char* doorNumStr = data["doornum"];

  if (doorNumStr) {
    String doorNumString(doorNumStr);
    std::vector<String> doorNums;
    int start = 0;
    int end = doorNumString.indexOf(';');
    while (end != -1) {
      doorNums.push_back(doorNumString.substring(start, end));
      start = end + 1;
      end = doorNumString.indexOf(';', start);
    }
    doorNums.push_back(doorNumString.substring(start));

    JsonDocument response; 
    actualState = BOXSTATE_OPENING;
    doorOpenTimeout = currentMillis + DOOR_OPENING_TIMEOUT;
    boxDisplay.writeResponseLine(RESPONSELINE_OPENING);
    disableExternal();

    for (const String& numStr : doorNums) {
      uint8_t num = numStr.toInt();
      if (num < DOOR_COUNT) {
        // Issue pulse to open the box
        gpioHal.openDoor(num);  // HAL should handle the timing of the pulse, i.e. turn on the lock for a short time and then turn it off, so we don't have to worry about it here, but we might want to add some error handling in case the door fails to open, e.g. by checking the state after a delay and retrying if it's not open, or by returning an error response to the UI
        gpioHal.ambientOn(); // pro test, zatím rozsvítí ambient světlo jako indikaci, že jsme zpracovali požadavek, než bude implementováno plánování odeslání stavu dveří
      }
    }
     //serializeJson(response, serializedChanges);
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
  
  if ( strcmp(pinCommand, PIN_DELETE_BEACON) == 0 ) {
		CacheRecord recToDelete;
    recToDelete.pinId = data["pinid"];
    if (data["pinname"] != nullptr) {
      pinId = data["pinid"];
      recToDelete.name[0] = '\0';
      strncat(recToDelete.name, data["pinname"], PIN_NAME_LEN); 
      if ( pinStorage.removePin(recToDelete) == pinId ) {
        boxDisplay.logPrint("Pin " + String(pinId) + " / " + String(recToDelete.name) + " deleted successfully");
        statusResponse["lastresult"] = "Pin " + String(pinId) + " / " + String(recToDelete.name) + " deleted successfully";
        webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
        dataChange = true;
      } else {
        boxDisplay.logPrint("Error deleting pin " + String(pinId) + " / " + String(recToDelete.name));
        statusResponse["lastresult"] = "Error deleting pin " + String(pinId) + " / " + String(recToDelete.name);
        webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
      }
    } else {
      boxDisplay.logPrint("Error deleting pin with id " + String(pinId) + ": pin name not provided");
      statusResponse["lastresult"] = "Error deleting pin with id " + String(pinId) + ": pin name not provided";
      webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
    }
  } else if ( strcmp(pinCommand, PIN_SAVE_BEACON) == 0 ) {
    CacheRecord recToSave;
    recToSave.pinId = pinId;
    recToSave.name[0] = '\0';
    if (data["pinname"] != nullptr) {
      strncat(recToSave.name, data["pinname"], PIN_NAME_LEN);
    } else {
      boxDisplay.logPrint("Error saving pin with id " + String(pinId) + ": pin name not provided");
      statusResponse["lastresult"] = "Error saving pin with id " + String(pinId) + ": pin name not provided";
      webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
      return;
    }
    if (data["datefrom"] != nullptr) {
      recToSave.validFrom = data["datefrom"];
    } else {
      recToSave.validFrom = DATE_FROM_UNLIMITED; // default value if not provided
    }
    if (data["dateto"] != nullptr) {
      recToSave.validTo = data["dateto"];
    } else {
      recToSave.validTo = DATE_TO_UNLIMITED; // default value if not provided
    }
    if (data["amount"] != nullptr) {
      recToSave.remaining = data["amount"];
    } else {
      recToSave.remaining = -1; // defaults to unlimited (-1), 0 means expired
    }
    if (pinId){                   // update existing pin
      if ( pinStorage.updatePin(recToSave) == pinId ) {
        boxDisplay.logPrint("Pin " + String(pinId) + " / " + String(recToSave.name) + " saved successfully");
        statusResponse["lastresult"] = "Pin " + String(pinId) + " / " + String(recToSave.name) + " saved successfully";
        webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
        dataChange = true;
      } else {
        boxDisplay.logPrint("Error saving pin " + String(pinId) + " / " + String(recToSave.name));
        statusResponse["lastresult"] = "Error saving pin " + String(pinId) + " / " + String(recToSave.name);
        webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
      } 
    } else {                      // add new pin
      uint32_t newPinId = pinStorage.addPin(recToSave);  
      if ( newPinId ) {
        boxDisplay.logPrint("Pin " + String(newPinId) + " / " + String(recToSave.name) + " added successfully");
        statusResponse["lastresult"] = "Pin " + String(newPinId) + " / " + String(recToSave.name) + " added successfully";
        webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
        dataChange = true;
      } else {
        boxDisplay.logPrint("Error adding new pin " + String(recToSave.name));
        statusResponse["lastresult"] = "Error adding new pin " + String(recToSave.name);
        webSocketManager.sendMessage(sender, COMM_CONTENT, statusResponse);
      }
    }
  }
  if (dataChange) {
    JsonDocument tableUpdate;
    String pinsTable;
    tableUpdate["pinrows"] = pinStorage.getPins(1 , 100, pinsTable);    //$$$$$$$$$ hardcoded for now, bude potreba implementovat mechanismus notifikace zmeny pintable pro UI - kazdy klient si stahne tu stranku, kterou zrovna zobrazuje, a ta stranka pak bude posilat WS zpravy s požadavkem na aktualizaci tabulky, např. při otevření dialogu pro správu pinů, nebo po každé změně pinů, aby se zajistilo, že UI bude mít aktuální data, ale zároveň se nebude přetěžovat WS zprávami s aktualizací tabulky pro všechny klienty pokaždé, když dojde ke změně pinů, což by mohlo být časté a nemusí být relevantní pro všechny klienty najednou, takže je lepší nechat to na jednotlivých klientech, aby si řekli o aktualizaci tabulky, když ji potřebují)
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

void getPresenceCode (char *PresenceCodePtr){
  for(int i = 0; i < PRESENCE_CODE_LENGTH; i++){
    PresenceCodePtr[i] = rand() % 10;
  }
  presenceCodeExpiration = currentMillis + PRESENCE_CODE_VALIDITY;
}

void setup() {
  Serial.begin(9600);
  Serial.println("\n\n --- B O X   prototype starting! ---\n");

// Initialize I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);  // klasika, žádný spěch

  boxDisplay.displayInit(&gpioHal);
  // boxDisplay.logPrint("Display initialized\n");  

  boxDisplay.logPrint("--- BOX prototype ---\n");

  // Initialize timer manager and GPIO HAL (GPIO HAL needs timer manager for scheduling future tasks, e.g. to turn off the lock after some time)
  timerManager.initializeTimerManager(handleDueActions);
  gpioHal.initializeGpioHAL(&timerManager);

  // Initializa keyboard (null operation at present)
  boxKeyboard.keyboardInit();

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
  // Start server
  webserver.begin();
  boxDisplay.logPrint("Webserver started!\n");

  // Start preview mode with 320x240 resolution
  // CAM_VIDEO_MODE_3 = 320x240 according to the enum
  Serial.println("Starting preview...");
  //myCAM.startPreview(CAM_VIDEO_MODE_3);    // Something strange here: header used by adruino IDE lists many modes, mode 3 means 320x240
  myCAM.startPreview(CAM_VIDEO_MODE_0);     // header used by Platformio lists only 4 modes, mode 0 means 320x240
  boxDisplay.logPrint("Streaming active\n");

//Websocket stuff initialization

  webSocketManager.initializeWebSocket(&webserver);
  Serial.println("WebSocket initialized successfully");
  webSocketManager.registerMessageHandler("COMM_SET_PIN",   handleSetPin);
  webSocketManager.registerMessageHandler("COMM_OPEN_BOX",   handleOpenBox);                  // open door
  webSocketManager.registerMessageHandler("COMM_GET_PINS",   handleGetPins);                  // get pin list 
  webSocketManager.registerMessageHandler("COMM_GET_DOORS",   handleGetDoors);                // get doors state
  webSocketManager.registerMessageHandler("COMM_GET_AMBIENT", handleGetAmbient);              // get ambient light state 
  
  /*
// set I/O pins
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  pinMode(doorPin, INPUT);
  */
  boxDisplay.logPrint("SETUP COMPLETE!\n");

}  //setup

void handleDueActions(uint8_t action, const char* arg) {
  // Implement the logic to perform actions based on the action code and argument
  // For example:
  switch (action) {
    case NOTIFY_DOOR_CHANGE: {
        JsonDocument response;
        uint8_t doorNum = atoi(arg);
        if (doorNum >= DOOR_COUNT) {
          boxDisplay.logPrint("Error: Invalid door number in timer callback: " + String(doorNum));
          return;
        }
        if (gpioHal.readDoorState(doorNum) == DOOR_MIXED) {
          boxDisplay.logPrint("Warning: Mixed state detected for door " + String(doorNum) + " during notification");
        }
        if (gpioHal.getLogDoorMapping(doorNum) != doorNum) {
          uint8_t logDoorNum = gpioHal.getLogDoorMapping(doorNum);
          for (uint8_t i = 0; i < DOOR_COUNT; i++) {
            if (gpioHal.getLogDoorMapping(i) == logDoorNum) {
              gpioHal.setDoorLastState(i, gpioHal.readDoorState(i));
            }
          }
        } else {
          gpioHal.setDoorLastState(doorNum, gpioHal.readDoorState(doorNum));
        }
        uint8_t isOpen = gpioHal.readDoorState(doorNum);
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
      }
      break;
    case LOCK_DEACTIVATE: {
        boxDisplay.logPrint(arg);
        uint8_t lockNum = atoi(arg);
        if (lockNum < DOOR_COUNT) {
          gpioHal.lockDeactivate(lockNum);
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
        boxDisplay.logPrint ("Password timeout passed\n");
        actualState = BOXSTATE_PASSWORD;
        keyboardState.currentPasswordLen=0;
        keyboardState.keyboardMode=KEYBOARD_MODE_PASSWORD;
        boxDisplay.writeActionLine(ACTIONLINE_OPEN_PASSWORD);

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

void handleKeyboard(){
// Keyboard input handling
  if ( boxKeyboard.pollKeyboard(&keyboardState) ){
    switch (actualState ){
      case BOXSTATE_HOME:
        if(keyboardState.keyboardMode == KEYBOARD_MODE_COMMAND) {
          if (keyboardState.currentKey == KEYBOARD_KEY_1) {     //switch to password
            keyboardState.currentKey = ' ';
            keyboardState.keyboardMode = KEYBOARD_MODE_PASSWORD;
            keyboardState.currentPasswordLen = 0;
            actualState = BOXSTATE_PASSWORD;
            boxDisplay.writeInfoLine (INFOLINE_HOME);
            boxDisplay.writeActionLine (ACTIONLINE_OPEN_PASSWORD);
            // ### sem se musi pridat blokace UI
          } else if (keyboardState.currentKey == KEYBOARD_KEY_2) { // switch to presence
            actualState = BOXSTATE_PRESENCE;
            getPresenceCode(presenceCode);
            boxDisplay.writeInfoLine(INFOLINE_CANCEL);
//            boxDisplay.writeActionLine(ACTIONLINE_PRESENCE);
            boxDisplay.setVerifyCode(presenceCode);
            boxDisplay.setVerifyCodeBar((presenceCodeExpiration - currentMillis)/PRESENCE_CODE_VALIDITY*100);
            boxDisplay.writeResponseLine(RESPONSELINE_PRESENCE);
          } else {
            Serial.println("Invalid command input");
          }
        } else {
          keyboardState.keyboardMode = KEYBOARD_MODE_COMMAND;                // shouldn't happen -> synchronize keyboard with overal state
        }
        break;
      case BOXSTATE_PASSWORD:
        if (keyboardState.keyboardMode == KEYBOARD_MODE_PASSWORD) {
          if (keyboardState.passwordComplete) {                               // password entered
            keyboardState.passwordComplete = false;
            doorToOpen = pinStorage.usePin(keyboardState.currentPassword);
            if (doorToOpen > 0) {                                            // valid PIN
              actualState = BOXSTATE_OPENING;
              gpioHal.openDoor(doorToOpen);
              doorOpenTimeout = currentMillis + DOOR_OPENING_TIMEOUT;
              boxDisplay.writeResponseLine(RESPONSELINE_OPENING);
              badPasswordCount = 0;
            } else {
              boxDisplay.writeResponseLine(RESPONSELINE_BADPASS);
              keyboardState.currentPasswordLen = 0;
              boxKeyboard.clearPassword(&keyboardState);
              if (badPasswordCount++ > PASS_ERR_MAXMULTIPLY){
                currentBadPasswordMillis = PASS_ERR_DELAY * PASS_ERR_MAXMULTIPLY * 1000;
                badPasswordDelayFinish = PASS_ERR_DELAY * PASS_ERR_MAXMULTIPLY * 1000 + currentMillis;
              } else {
                currentBadPasswordMillis = PASS_ERR_DELAY * badPasswordCount * 1000;
                badPasswordDelayFinish = PASS_ERR_DELAY * badPasswordCount * 1000 + currentMillis;
              }
              timerManager.scheduleOnce(currentBadPasswordMillis, PASSWORD_TIMEOUT);
            }
            boxKeyboard.clearPassword(&keyboardState);
            keyboardState.currentPasswordLen = 0;
          } 
        } else {
          keyboardState.keyboardMode = KEYBOARD_MODE_PASSWORD;
        }
        break;
      case BOXSTATE_BADPASS:
        if(keyboardState.keyboardMode == KEYBOARD_MODE_COMMAND) {
          if (keyboardState.currentKey == KEYBOARD_KEY_CANCEL) {     //cancel - retorun to HOME
            keyboardState.currentKey = ' ';
            actualState = BOXSTATE_HOME;
            boxDisplay.writeInfoLine (INFOLINE_HOME);
            boxDisplay.writeActionLine (ACTIONLINE_HOME);
            boxDisplay.writeResponseLine (RESPONSELINE_HOME);
            // ### sem se musi pridat blokace UI
          }
        } else {
          keyboardState.keyboardMode = KEYBOARD_MODE_COMMAND;                // shouldn't happen -> synchronize keyboard with overal state
        }
        break;
      }
    }
  } //handleKeyboard

void loop() {
  JsonDocument changes;
  currentMillis = millis();
    
  switch (actualState){
    case BOXSTATE_HOME:
      enableExternal();
      handleKeyboard();
      webSocketManager.cleanupConnections();
      ElegantOTA.loop();
      if (currentMillis > linkStatusMillis) {
        checkLinkStatus();
      }
      break;
    case BOXSTATE_PASSWORD:
      handleKeyboard();
      if (badPasswordDelayFinish < currentMillis) {             // time for password entry expired
        if (actualState == BOXSTATE_PASSWORD){                // avoiding situation when timeous occurs very close to finishing the password
          actualState = BOXSTATE_HOME;
          boxDisplay.writeActionLine(ACTIONLINE_HOME);
          boxDisplay.writeResponseLine(RESPONSELINE_HOME);
          boxDisplay.writeInfoLine(INFOLINE_HOME);
          enableExternal();
        }
      } else {                                                // continue entering password
        if (displayActionMillis < currentMillis) {
          disableExternal();
          boxDisplay.writeActionLine(ACTIONLINE_HOME);
          boxDisplay.setVerifyCodeBar((badPasswordDelayFinish - currentMillis) / currentBadPasswordMillis * 100);
          boxDisplay.writeResponseLine(RESPONSELINE_PRESENCE);
          displayActionMillis = currentMillis + ACTIONLINE_REFRESH_INTERVAL; 
        }
      }
      break;
    case BOXSTATE_BADPASS:
      if (displayActionMillis < currentMillis) {
        boxDisplay.writeActionLine(ACTIONLINE_WAIT);
      }
      if (badPasswordDelayFinish < currentMillis) {
        boxDisplay.writeActionLine(ACTIONLINE_OPEN_PASSWORD);
        boxDisplay.writeInfoLine(INFOLINE_PASS);
      }
      break;
    case BOXSTATE_PRESENCE:
      if (displayActionMillis < currentMillis) {
        boxDisplay.setVerifyCodeBar((presenceCodeExpiration - currentMillis)/PRESENCE_CODE_VALIDITY*100);
        boxDisplay.writeResponseLine(RESPONSELINE_PRESENCE);
        displayActionMillis = currentMillis + ACTIONLINE_REFRESH_INTERVAL;
      }
      if (presenceCodeExpiration < currentMillis) {
        actualState = BOXSTATE_HOME;
        boxDisplay.writeActionLine(ACTIONLINE_HOME);
        boxDisplay.writeResponseLine(RESPONSELINE_HOME);
        boxDisplay.writeInfoLine(INFOLINE_HOME);
      }
      break;
      case BOXSTATE_OPENING:
        if (gpioHal.readDoorState(doorToOpen) == DOOR_OPEN) {
          actualState = BOXSTATE_OPEN;
          boxDisplay.writeActionLine(ACTIONLINE_CLOSE);
          boxDisplay.writeInfoLine(INFOLINE_EMPTY);
          boxDisplay.writeResponseLine(RESPONSELINE_EMPTY);
          cameraStart();
          gpioHal.ambientOn();

        } else {
          if (currentMillis > doorOpenTimeout) {
            boxDisplay.logPrint("Error - door" + String(doorToOpen) + "not openned");
            actualState = BOXSTATE_HOME;
            boxDisplay.writeActionLine(ACTIONLINE_HOME);
            boxDisplay.writeResponseLine(RESPONSELINE_HOME);
            boxDisplay.writeInfoLine(INFOLINE_HOME);
            cameraStop();
            enableExternal();
          }
        }
        break;
      case BOXSTATE_OPEN:
        if (gpioHal.readDoorState(doorToOpen) == DOOR_CLOSED) {
            actualState = BOXSTATE_CLOSED;
            boxDisplay.writeActionLine(ACTIONLINE_CLOSETHX);
            boxDisplay.writeInfoLine(INFOLINE_EMPTY);
            boxDisplay.writeResponseLine(RESPONSELINE_EMPTY);
            ambientOffTimeout = currentMillis + AMBIENT_TIMEOUT;
        }
      // je otazkou, jak resit, kdyz se dvere nezavrou, asi nejlepsi je nechat to byt a az nekdo dorazi, tak nejdriv zavre a pak se bude pokracovat v krasojizde
      // coz by odpovidalo tomu, ze se zde neudela nic
      // v advanced verzi se dvere oznaci za vadne a prestane se s nimi pracovat
      // mozna zastavit kameru po nejakem case
        break;
      case BOXSTATE_CLOSED:
        if (ambientOffTimeout < currentMillis) {
          actualState = BOXSTATE_HOME;
          gpioHal.ambientOff();
          cameraStop();
          boxDisplay.writeActionLine(ACTIONLINE_HOME);
          boxDisplay.writeResponseLine(RESPONSELINE_HOME);
          boxDisplay.writeInfoLine(INFOLINE_HOME);
          enableExternal();
        }
        break;
      case BOXSTATE_EXTERNAL:
        // UI command running
        break;
  }
  if ( statusLineTimeout < currentMillis ) {
    statusLineTimeout = currentMillis + STATUSLINE_REFRESH_INTERVAL;
    boxDisplay.writeStatusLine();
  }
// captureThread() processes incoming data and calls our callback
//  myCAM.captureThread();

} // loop