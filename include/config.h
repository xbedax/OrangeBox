#define _VERSION_ "1.01.b"
#if __has_include("box_setup.h")
#include "box_setup.h"
#endif

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
#ifndef ACTIVE_DOOR_NUMBERS
#define ACTIVE_DOOR_NUMBERS  1                          // Number of active doors, has to correspond with INITIAL_DOOR_MAPPING
#endif
#ifndef INITIAL_DOOR_MAPPING
#define INITIAL_DOOR_MAPPING  {  {0, 1, 39, 0, 0}  \
                                                  }
#endif
#ifndef AMBIENT_PIN
#define AMBIENT_PIN 0
#endif
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

// QR / barcode scanner UART settings. The actual module command bytes are
// intentionally configurable because available documentation is incomplete.
#ifndef QR_SCANNER_UART_NUM
#define QR_SCANNER_UART_NUM 0
#endif
#ifndef QR_SCANNER_BAUD
#define QR_SCANNER_BAUD 9600
#endif
#ifndef QR_SCANNER_RX_PIN
#define QR_SCANNER_RX_PIN 44
#endif
#ifndef QR_SCANNER_TX_PIN
#define QR_SCANNER_TX_PIN 43
#endif
#ifndef QR_SCANNER_SWITCH_PIN
#define QR_SCANNER_SWITCH_PIN 255
#endif
#ifndef QR_SCANNER_SWITCH_ACTIVE_HIGH
#define QR_SCANNER_SWITCH_ACTIVE_HIGH 1
#endif
#ifndef QR_SCANNER_ACTIVATE_COMMAND
#define QR_SCANNER_ACTIVATE_COMMAND "hex: 7E 00 08 01 00 02 01 AB CD"
#endif
#ifndef QR_SCANNER_DEACTIVATE_COMMAND
#define QR_SCANNER_DEACTIVATE_COMMAND ""
#endif
#ifndef QR_SCANNER_RESPONSE_WINDOW_MS
#define QR_SCANNER_RESPONSE_WINDOW_MS 300
#endif
#ifndef QR_SCANNER_COMMAND_RESPONSE_WINDOW_MS
#define QR_SCANNER_COMMAND_RESPONSE_WINDOW_MS 500
#endif
#ifndef QR_SCANNER_FRAME_IDLE_MS
#define QR_SCANNER_FRAME_IDLE_MS 40
#endif
#ifndef QR_SCANNER_SCAN_TIMEOUT_MS
#define QR_SCANNER_SCAN_TIMEOUT_MS 30000
#endif
#ifndef QR_SCANNER_STUB_CONSOLE_BAUD
#define QR_SCANNER_STUB_CONSOLE_BAUD 9600
#endif

// Wifi connection settings
// Valid values for WIFI_SSID and WIFI_PASSWD should be provided in the build environment or in a separate configuration file. If not defined, default values will be used.
#ifndef BOX_HOST_NAME
#define BOX_HOST_NAME "BOX-test"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID "test-SSID"
#endif
#ifndef WIFI_PASSWD
#define WIFI_PASSWD "test-Key"
#endif

// VPN / reverse proxy routing
// Apache currently identifies a box instance by this remote forward port.
// Override VPN_REMOTE_BIND_PORT in credentials.h for each deployed box.
#ifndef VPN_REMOTE_BIND_PORT
#define VPN_REMOTE_BIND_PORT 8084
#endif
// Keep these values in sync with sshd ClientAliveInterval/ClientAliveCountMax.
// The client interval intentionally lands after the server stale-session window.
#ifndef VPN_SERVER_CLIENT_ALIVE_INTERVAL_SEC
#define VPN_SERVER_CLIENT_ALIVE_INTERVAL_SEC 7
#endif
#ifndef VPN_SERVER_CLIENT_ALIVE_COUNT_MAX
#define VPN_SERVER_CLIENT_ALIVE_COUNT_MAX 3
#endif
#ifndef VPN_SERVER_STALE_SESSION_WINDOW_SEC
#define VPN_SERVER_STALE_SESSION_WINDOW_SEC \
  (VPN_SERVER_CLIENT_ALIVE_INTERVAL_SEC * VPN_SERVER_CLIENT_ALIVE_COUNT_MAX)
#endif
#ifndef VPN_CLIENT_KEEPALIVE_MARGIN_SEC
#define VPN_CLIENT_KEEPALIVE_MARGIN_SEC 9
#endif
#ifndef VPN_CLIENT_KEEPALIVE_INTERVAL_SEC
#define VPN_CLIENT_KEEPALIVE_INTERVAL_SEC \
  (VPN_SERVER_STALE_SESSION_WINDOW_SEC + VPN_CLIENT_KEEPALIVE_MARGIN_SEC)
#endif
#ifndef VPN_STALE_REMOTE_LISTENER_GRACE_SEC
#define VPN_STALE_REMOTE_LISTENER_GRACE_SEC 14
#endif
#ifndef VPN_STALE_REMOTE_LISTENER_COOLDOWN_MS
#define VPN_STALE_REMOTE_LISTENER_COOLDOWN_MS \
  ((VPN_SERVER_STALE_SESSION_WINDOW_SEC + VPN_STALE_REMOTE_LISTENER_GRACE_SEC) * 1000UL)
#endif

// NTP settings
#ifndef NTP_SERVER
#define NTP_SERVER "pool.ntp.org"
#endif
#ifndef GMT_OFFSET_SEC
#define GMT_OFFSET_SEC 3600                 // Adjust according to timezone
#endif
#ifndef DAYLIGHT_OFFSET_SEC
#define DAYLIGHT_OFFSET_SEC 3600            // Adjust according to daylight saving time
#endif
#ifndef RTC_NTP_SYNC_INTERVAL_MS
#define RTC_NTP_SYNC_INTERVAL_MS 3600000UL  // Refresh RTC from NTP once per hour
#endif
#ifndef RTC_MIN_VALID_UNIX_TIME
#define RTC_MIN_VALID_UNIX_TIME 1704067200UL // 2024-01-01, reject unset RTC values
#endif
#ifndef RTC_I2C_ADDRESS
#define RTC_I2C_ADDRESS 0x68                 // DS3231 default I2C address
#endif

// Websocket settings
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9


// Log server settings
#ifndef LOG_SERVER_IP
#define LOG_SERVER_IP "192.168.1.100"
#endif
#ifndef LOG_SERVER_PORT
#define LOG_SERVER_PORT 514
#endif

/*-----------------*/
/* Global constants*/
/*-----------------*/

// Command strings for WebSocket communication
#define CMD_COUNT 15
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
#define COMM_GET_DIAGNOSTICS "get_diagnostics"
#define COMM_SNAPSHOT_DIAGNOSTICS "snapshot_diagnostics"
#define COMM_GET_DIAGNOSTIC_SNAPSHOTS "get_diagnostic_snapshots"
#define COMM_ENORDIS "c_enordis"
#define COMM_LOG "c_log"


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
