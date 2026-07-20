#ifndef BOX_DISPLAY_H
#define BOX_DISPLAY_H
#include "config.h"
#include <Arduino.h>
#include "gpio_hal.h"
//#include <LCDI2C_Multilingual.h>

//#ifdef DISP_OLED
//#include <Adafruit_GFX.h>
//#include <Adafruit_SH1106.h>
//#define OLED_ADDRESS 0x3C  //hard codded in Adafruit_SH1106.h SH1106_I2C_ADDRESS

//#define OLEDROWS 4
//#define OLEDTEXTSIZE 8
//#define OLEDLINECHARS 20
//#endif

//overal layout - 20x4 lcd
#define INFO_LINE_IDX 0
#define ACTION_LINE_IDX 1
#define RESPONSE_LINE_IDX 2
#define STATUS_LINE_IDX 3                               //position of the status line on the LCD, adjust as needed


// Info line layout
#define INFOIDX_MAX      4                              // number of info line types, adjust as needed, but make sure to update the info line content and labels accordingly
#define INFOLINE_TXT_HOME      " 1-heslo  2-overeni "   // Info line message for mode selection
#define INFOLINE_TXT_PASS      "*-oprava #-potvrzeni"   // Info line message for password input mode
#define INFOLINE_TXT_CANCEL    "*-zpet              "                 // Info line message for cancel
#define INFOLINE_TXT_EMPTY     "                    "

#define INFOLINE_HOME      0                            // Info line message for mode selection
#define INFOLINE_PASS      1                            // Info line message for password input mode
#define INFOLINE_CANCEL    2                            // Info line message for cancel
#define INFOLINE_EMPTY     3

// Content of Action line - modes
#define ACTIONIDX_MAX    6                              // number of action line types, adjust as needed, but make sure to update the action line content and labels accordingly

#define ACTIONLINE_OPEN_PASSWORD              0         // Action: Password input
#define ACTIONLINE_BADPASS                    1         // Action: Waiting for next password input
#define ACTIONLINE_CLOSE                      2         // Action: Close door
#define ACTIONLINE_PRESENCE                   3         // Action: Presence code display
#define ACTIONLINE_HOME                       4         // Action: No action
#define ACTIONLINE_CLOSETHX                   5         // Action: Door closed

#define ACTIONLINE_TXT_PASS      "   Heslo: "           // Action line message for password input mode
#define ACTIONLINE_TXT_BADPASS   "      Cekejte!      " // Action line message for waiting for password input mode
#define ACTIONLINE_TXT_CLOSE     "   Zavrete dvere!   " // Action line message for closing doors
#define ACTIONLINE_TXT_PRESENCE  "   Overeni: "         // Action line message for presence detection
#define ACTIONLINE_TXT_HOME      ""
#define ACTIONLINE_TXT_CLOSETHX  "      Dekujeme!     " // Action line message for door closed

// Content of Response line - responses to user actions
#define RESPONSEIDX_MAX 5                               // number of response line types, adjust as needed, but make sure to update the response line content and labels accordingly

#define RESPONSELINE_OPENING                 0          // Response: Door opening
#define RESPONSELINE_BADPASS                 1          // Response: Bad password
#define RESPONSELINE_PROGRESS                2          // Response: Presence code verification
#define RESPONSELINE_HOME                    3          // Response: No action
#define RESPONSELINE_EMPTY                   4


#define RESPONSELINE_TXT_OPENING "OTEVIRAM"             // Response line message for door opening action
#define RESPONSELINE_TXT_BADPASS "SPATNE HESLO!"        // Response line message for bad password
#define RESPONSELINE_TXT_PROGRESS " ZBYVA: "
#define RESPONSELINE_TXT_HOME       _VERSION_
#define RESPONSELINE_TXT_EMPTY    "                    "

#define PROGRESS_BAR_WIDTH                  10          // Width of the presence code validity progress bar, adjust as needed based on display width and layout

// Status line stuff
// line layout
#define STIDX_CLIENT     0
#define STIDX_LINK       1
#define STIDX_AMBIENT    2
#define STIDX_ONLINE     3   
#define STIDX_MAX        4                              // total number of status indicators, adjust as needed, but make sure to update the status line layout in the display accordingly

//  connection indicators ... LINK
#define ONLINE_STATUS_WIFI          'W'         // WiFi connected
#define ONLINE_STATUS_5G            '5'         // 5G connected (if applicable)
#define ONLINE_STATUS_OFFLINE       'X'         // No connection

//  communication indicators ... COMM
#define COMMUNICATION_STATUS_OK        'O'         // Communication OK
#define COMMUNICATION_STATUS_ERROR     'E'         // Communication error



//ambient ... AMBIENT
#define AMBIENT_STATUS_ON             'A'         // Ambient light on
#define AMBIENT_STATUS_OFF            '.'         // Ambient light off

//display libraries - lcdI2C
#ifdef DISP_LCD
#define LCD_ADDRESS                 0x27
#define LCD_WIDTH                   20
#define LCD_HEIGHT                  4
#define DISP_WIDTH                  LCD_WIDTH
#define DISP_DOOR_COUNT  (LCD_WIDTH - (STIDX_MAX + 3))/2  // number of characters available for door status display, adjust as needed, but make sure to update the status line layout in the display accordingly

#endif




// Status line indicator indexes




class BoxDisplay {

public:
    // Box display interface
// This header defines the shared display API for OLED/LCD output.

// Initialize the selected display hardware.
void displayInit(GpioHAL* gpioHal);

// Display stars for password input, with length up to PASS_MAX.
// void displayPassword(uint8_t pass_len);

// Whether there is somebody connected to websoc server.
void setCommunicationStatus (char comm_status);
// Whether the box is online (connected to WiFi or 5G).
void setLinkStatus (char link_status);
// Set list of currently open doors, e.g. "1,3" for doors 1 and 3 open, empty string for all closed
void setOpenDoorList ( void);
// Set the ambient light status, e.g. 'A' for on, '.' for off
void setAmbientStatus (char ambient_status);
// Set the current password length for display purposes, e.g. when the user is entering the password, this can be used to display the progress of password input
void setPasswordLength (uint8_t pass_len);
// Set the current presence code for display purposes, e.g. when the user is verifying their presence code, this can be used to display the code and its validity progress
void setVerifyCode (const char* code);
void setProgressBar (uint8_t expirationProgresss); // e.g. display a progress bar for the presence code validity, where expirationProgress is a value from 0 to 100 representing the percentage of validity time remaining

void writeInfoLine (uint8_t infoType);
void writeStatusLine (void);
void writeActionLine (uint8_t actionType);
void writeResponseLine (uint8_t responseType);


private:

const char *infoTypes[INFOIDX_MAX] = { INFOLINE_TXT_HOME, INFOLINE_TXT_PASS, INFOLINE_TXT_CANCEL, INFOLINE_TXT_EMPTY };  // Labels for the status indicators, e.g. Client, Link, Ambient, Online
const char *actionTypes[ACTIONIDX_MAX] = { ACTIONLINE_TXT_PASS, ACTIONLINE_TXT_BADPASS, ACTIONLINE_TXT_CLOSE, ACTIONLINE_TXT_PRESENCE, ACTIONLINE_TXT_HOME, ACTIONLINE_TXT_CLOSETHX };  // Labels for the action indicators
const char *responseTypes[RESPONSEIDX_MAX] = { RESPONSELINE_TXT_OPENING, RESPONSELINE_TXT_BADPASS, RESPONSELINE_TXT_PROGRESS, RESPONSELINE_TXT_HOME, RESPONSELINE_TXT_EMPTY };  // Labels for the response indicators
char currentStatus[STIDX_MAX] = {'C', 'E', '.', 'X'};              // Array to hold the current status indicators, e.g. online status, communication status, ambient status, etc.
char currentDoorOpen[DISP_DOOR_COUNT*2+1];// List of currently open doors, e.g. [0103] for doors 1 and 3 open, door 2 closed, adjust size as needed based on the number of doors and display layout
int8_t currentPassLen;                      // The length of password entered so far, used for display purposes
char currentVerifyCode[PASS_MAX + 1];       // The current presence code being displayed, used for display purposes, +1 for null terminator
int8_t currentProgress;           // Progress of the presence code validity, e.g. for display in a progress bar, value from 0 to 100 representing the percentage of validity time remaining
int8_t nextPasswordWait;                    // Time to wait for the next password input, e.g. after a failed attempt, in seconds  

// Center text for an LCD line or OLED row.
String centerLine(String text);
// Elements of display interface manipulations
GpioHAL *gpioHAL; // Pointer to the GPIO HAL instance, used for getting door status for display purposes
#ifdef DISP_LCD
//LCDI2C_Latin lcd1{LCD_ADDRESS, LCD_WIDTH, LCD_HEIGHT};
#endif


};

#endif // BOX_DISPLAY_H
