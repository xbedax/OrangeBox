#define _VERSION_ "1.01.a"


/*-----------------------*/
/* Configuration options */
/*-----------------------*/

/* Components - build controls*/
// Enable to use LCD display
#define DISP_LCD
// Enable to use OLED display
#define DISP_OLED
// Enable to use SPI camera (Arduino Mega)
#define CAM_SPI
// Enable to use ESP32 camera
#define CAM_ESP

/*-----------------*/
/* Global settings */
/*-----------------*/
// PIN settings
#define PIN_NAME_LEN 10                      //length of PIN identificator
#define PIN_CODE_LEN 8                      // lenght of the PIN itself


// Box password max lenght
#define PASS_MAX PIN_CODE_LEN
#define PASS_ERR_DELAY          20          //sec
#define PASS_ERR_MAXMULTIPLY    4
#define PASS_ENTRY_TIMEOUT      30          //sec: if password not fully entered within timeout, the entry is discarded and state changet do HOME


#define ACTIONLINE_REFRESH_INTERVAL 1000    //ms
#define STATUSLINE_REFRESH_INTERVAL 500     //ms


#define LINK_CHECK_INTERVAL 3000

// Wifi connection settings

#define BOX_HOST_NAME "BOX-007"
#ifndef WIFI_SSID
#define WIFI_SSID "someSSID"
#endif
#ifndef WIFI_PASSWD
#define WIFI_PASSWD "someKey"
#endif

// NTP settings
#define NTP_SERVER "tak.cesnet.cz"
#define GMT_OFFSET_SEC 3600                 // Adjust according to timezone
#define DAYLIGHT_OFFSET_SEC 3600            // Adjust according to daylight saving time

// Websocket settings
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9


/*-----------------*/
/* Global constants*/
/*-----------------*/

// Command strings for WebSocket communication
#define COMM_GET_DOOR_STATE "get_door"
#define COMM_WATCHDOG "watchdog"
#define COMM_SET_PIN "set_pin"
#define COMM_PIN_DELETE "pinDelete"
#define COMM_GET_AMBIENT "get_ambient"
#define COMM_GET_PINS "get_pins"
#define COMM_GET_PIN_INFO "get_PinInfo"
#define COMM_OPEN_BOX "opendoor"
#define COMM_VISIBILITY "c_visibility"
#define COMM_CONTENT "c_content"

#define WATCHDOG_INTERVAL 10000
#define WATCHDOG_TIMEOUT 30000 


// Html beacons

#define PIN_DELETE_BEACON "deletebutton"
#define PIN_SAVE_BEACON "savebutton"

#define AMBIENT_ON_BEACON "ambient_state_on"
#define AMBIENT_OFF_BEACON "ambient_state_off"

#define DOOR_OPEN_BEACON "door_state_open"
#define DOOR_CLOSED_BEACON "door_state_closed"
#define DOOR_MIXED_BEACON "door_state_mixed"

#define STATE_NEGATIVE "no"
#define STATE_POSITIVE "yes"

#define DATE_TO_UNLIMITED 2147483647            // Timestamp for 2030-12-31, used to represent unlimited validity for pins
#define DATE_FROM_UNLIMITED 1735689600          // Timestamp for 2025-01-01, used to represent unlimited validity for pins

#define PRESENCE_CODE_LENGTH 6                  // Length of the presence code, adjust as needed, but make sure to update the generation and verification logic accordingly
#define PRESENCE_CODE_VALIDITY 300              // seconds, i.e. 5 minutes, after this time the presence code is considered expired and the user is considered not present, unless a new presence code is received