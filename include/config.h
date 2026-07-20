#define _VERSION_ "1.01.a"
#include "credentials.h"


/*-----------------------*/
/* Configuration options */
/*-----------------------*/

/* Components - build controls*/
// Enable to use LCD display
#define DISP_LCD
// Enable to use OLED display
#ifndef BOX_SIMULATION
#define DISP_OLED
#endif
// Enable to use SPI camera (Arduino Mega)
#define CAM_SPI
// Enable to use ESP32 camera
//#define CAM_ESP

/*-----------------*/
/* Door mappings */
/*-----------------*/
// Logical Door Number, lock GPIO pin, door state pin, extender Number, last State
#define ACTIVE_DOOR_NUMBERS  1                          // Number of active doors, has to correspond with INITIAL_DOOR_MAPPING
#define INITIAL_DOOR_MAPPING  {  {0, 0, 3, 0, 0}  \
                                                  }
#define AMBIENT_PIN 11

/*-----------------*/
/* Global settings */
/*-----------------*/
// PIN settings
#define PIN_NAME_LEN 10                      //length of PIN identificator
#define PIN_CODE_LEN 8                      // lenght of the PIN itself


// Password entry settings
#define PASS_MAX PIN_CODE_LEN
#define PASS_ERR_DELAY          20          //sec
#define PASS_ERR_MAXMULTIPLY    4
#define PASS_ENTRY_TIMEOUT      30          //sec: if password not fully entered within timeout, the entry is discarded and state changet do HOME

// Ambient light settings
#define AMBIENT_TIMEOUT         10000       // how long to keep ambient light on after door closes, in milliseconds


// Presence code settings
#define PRESENCE_CODE_VALIDITY  30          // sec - from config.h (5 minutes)
#define PRESENCE_CODE_LENGTH 6                  // Length of the presence code, adjust as needed, but make sure to update the generation and verification logic accordingly


#define ACTIONLINE_REFRESH_INTERVAL 1000    //ms
#define STATUSLINE_REFRESH_INTERVAL 500     //ms


#define LINK_CHECK_INTERVAL 3000

// Wifi connection settings
// Valid values for WIFI_SSID and WIFI_PASSWD should be provided in the build environment or in a separate configuration file. If not defined, default values will be used.
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
#define CMD_COUNT 12
#define COMM_GET_DOOR_STATE "get_door"
#define COMM_WATCHDOG_PING "_ping_"
#define COMM_WATCHDOG_PONG "_pong_"
#define COMM_SET_PIN "set_pin"
#define COMM_GET_AMBIENT "get_ambient"
#define COMM_GET_PINS "get_pins"
#define COMM_GET_PIN_INFO "get_PinInfo"
#define COMM_OPEN_BOX "opendoor"
#define COMM_VISIBILITY "c_visibility"
#define COMM_CONTENT "c_content"
#define COMM_GET_PAGER "get_pager"
#define COMM_ENORDIS "c_enordis"


#define DEFAULT_MAX_WS_CLIENTS 15
#define WATCHDOG_INTERVAL 10000
#define WATCHDOG_TIMEOUT 30000 
#define WEBSOCKET_UPDATE_INTERVAL (WATCHDOG_INTERVAL / 4)


// Html beacons

#define PIN_DELETE_BEACON "deletebutton"
#define PIN_SAVE_BEACON "savebutton"

#define AMBIENT_ON_BEACON "ambient_state_on"
#define AMBIENT_OFF_BEACON "ambient_state_off"

#define DOOR_OPEN_BEACON "door_state_open"
#define DOOR_CLOSED_BEACON "door_state_closed"
#define DOOR_MIXED_BEACON "door_state_mixed"
#define DOOR_CONTROLS_BEACON "door_controls"

#define STATE_NEGATIVE "no"
#define STATE_POSITIVE "yes"

#define DATE_TO_UNLIMITED 2147483647            // Timestamp for 2030-12-31, used to represent unlimited validity for pins
#define DATE_FROM_UNLIMITED 1735689600          // Timestamp for 2025-01-01, used to represent unlimited validity for pins

// Message strings for WebSocket communication

#define MSG_PINID "pinid"
#define MSG_PINNAME "pinname"
#define MSG_PINVALUE "pinvalue"
#define MSG_PINVALIDFROM "datefrom"
#define MSG_PINVALIDTO "dateto"
#define MSG_PINUNLIMITED "checkbox-unlimited"
#define MSG_DOORNUM "doornum"
#define MSG_REMAINING "amount"
#define MSG_LASTRESULT "lastresult"
#define MSG_AMBIENTSTATE "ambient_state"
#define MSG_PRESENCECODE "presence"
#define MSG_CHECKPRESENCE "checkbox-presence"
#define MSG_DISABLE "disable"
#define MSG_ENABLE "enable"

/*--------------------*/
/* Technical settings */
/*--------------------*/

// Logging settings
#define SEVERITY_BUSSINESS      1
#define SEVERITY_INFO           2
#define SEVERITY_ERROR          3
#define SEVERITY_WARNING        4
#define SEVERITY_DEBUG          5
#define LOGPRINT_SEVERITY_LEVEL SEVERITY_DEBUG
