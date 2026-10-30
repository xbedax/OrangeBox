#include "config.h"
//#include <time.h>
#include <sstream>
#include <ctime>
#include "box_display.h"
#include "gpio_hal.h"
#include "logger.h"

#ifdef DISP_LCD
#include <LCDI2C_Multilingual.h>
static LCDI2C_Latin lcd1(LCD_ADDRESS, LCD_WIDTH, LCD_HEIGHT);  // I2C address: 0x27; LCD = Surenoo SLC2004A (EU / Latin)
#endif

//#ifdef DISP_OLED
//Adafruit_SH1106 display(OLED_ADDRESS);
//#endif

extern const char* fversion;

static constexpr int TEXT_LINE_COUNT = DISPLAYTEXT_MAX;
static constexpr  char const* langDisplayText[][TEXT_LINE_COUNT] = {
    {
        " 1-Pin 2-code 3-scan",
        " *-backspace #-enter",
        " *-cancel           ",
        "                    ",
        " Password: ",
        "    Please wait!    ",
        "   Close the door!  ",
        "  Verification: ",
        "",
        "     Thank you!     ",
        "  Scan the package. ",
        "OPENING",
        "BAD PASSWORD",
        "REMAINING: ",
        _VERSION_ + __COUNTER__,
        "                    ",
        "INVALID CODE"
    },
/*    {
        " 1-Pin 2-kód 3-sken ",
        "*-oprava #-potvrzení",
        "*-zpět              ",
        "                    ",
        "   Heslo: ",
        "      Čekejte!      ",
        "   Zavřete dveře!   ",
        "   Ověření: ",
        "",
        "      Děkujeme!     ",
        "  Naskenujte balík. ",
        "OTEVÍRÁM",
        "ŠPATNÉ HESLO",
        "ZBÝVÁ: ",
        _VERSION_,
        "                    ",
        "NESPRÁVNÝ KÓD"
    }*/
       {
        " 1-Pin 2-kod 3-sken ",
        "*-oprava #-potvrzeni",
        "*-zpet              ",
        "                    ",
        "   Heslo: ",
        "      Cekejte!      ",
        "   Zavrete dvere!   ",
        "   Overeni: ",
        "",
        "      Dekujeme!     ",
        "  Naskenujte balik. ",
        "OTEVIRAM",
        "SPATNE HESLO",
        "ZBYVA: ",
        _VERSION_,
        "                    ",
        "NESPRAVNY KOD"
    }
    /*,{
        " 1-Pin 2-Code 3-Scan",
        " *-Rückt, #-Eingabet",
        " *-Abbrechen        ",
        "                    ",
        " Passwort: ",
        "    Bitte warten!    ",
        " Schließen die Tür!  ",
        "  Überprüfung: ",
        "",
        "     Vielen Dank!    ",
        "  Scannen das Paket  ",
        "ÖFFNEN",
        "FALSCHES PASSWORT",
        "VERBLEIBEND: ",
        _VERSION_,
        "                    ",
        "FALSCHER CODE"
    }*/
      ,{
        " 1-Pin 2-Code 3-Scan",
        " *-Ruckt, #-Eingabet",
        " *-Abbrechen        ",
        "                    ",
        " Passwort: ",
        "    Bitte warten!    ",
        " Schliesen die Tur!  ",
        "  Uberprufung: ",
        "",
        "     Vielen Dank!    ",
        "  Scannen das Paket  ",
        "OFFNEN",
        "FALSCHES PASSWORT",
        "VERBLEIBEND: ",
        _VERSION_,
        "                    ",
        "FALSCHER CODE"
    }
};
constexpr int DISPLAY_LANG_COUNT = sizeof(langDisplayText) / sizeof(langDisplayText[0]);

String BoxDisplay::centerLine(String text) {
  uint8_t paddLen = 0;
  String centerText = "";
  
#ifdef DISP_LCD
    centerText = text.substring(0, DISP_WIDTH);
    uint8_t textLen = centerText.length();
    if (DISP_WIDTH >= textLen) {
        paddLen = (uint8_t)(DISP_WIDTH - textLen) / 2;
        for (uint8_t i = 0; i < paddLen; i++) {
            centerText = " " + centerText + " ";
        }
        if (centerText.length() < DISP_WIDTH) {
            centerText += " ";
        }
    }
#endif
    return centerText;
}

/*  No longer necessary, use writeActionLine with ACTIONLINE_PASS and currentPassLen to display the password input progress instead, but keeping this function here for reference if needed in the future
void BoxDisplay::displayPassword(uint8_t pass_len) {
  String passDisplay = "";
  for (uint8_t i = 0; i < pass_len && i < PASS_MAX; i++) {
    passDisplay += "*";
  }
  // Display the password on the selected display
#ifdef DISP_OLED
  display.setCursor(0, 20);
  display.print(passDisplay);
#endif
#ifdef DISP_LCD
  lcd1.setCursor(5, 3);
  lcd1.print("          ");
  uint8_t passPos = 10 - round( pass_len / 2);
  for (uint8_t i = 0; i < pass_len; i++) {
    lcd1.setCursor(passPos + i, 3);
    lcd1.print("*");
  }
#endif
}
*/


void BoxDisplay::writeStatusLine () {
    char statusLine[DISP_WIDTH + 1]; // Buffer to hold the entire status line, adjust size as needed
    const uint8_t statusStart = DISP_WIDTH - STIDX_MAX;
    uint8_t LineIdx = 0;

    memset(statusLine, ' ', DISP_WIDTH);
    statusLine[LineIdx++] = '['; // Initialize the string

    uint8_t doorCharsAvailable = statusStart > 2 ? statusStart - 2 : 0;
    uint8_t doorChars = min((uint8_t)strlen(currentDoorOpen), doorCharsAvailable);
    memcpy(&statusLine[LineIdx], currentDoorOpen, doorChars);
    LineIdx += doorChars;

    statusLine[LineIdx++] = ']';
    memcpy(&statusLine[statusStart], currentStatus, STIDX_MAX);
    statusLine[DISP_WIDTH] = '\0';   // Null-terminate the string
#ifdef DISP_LCD
    lcd1.setCursor(0, STATUS_LINE_IDX);
    lcd1.print(statusLine);
#endif
} // writeStatusLine

void BoxDisplay::writeActionLine (DisplayText actionType) {
  char actionLineBuff[DISP_WIDTH + 1]; // Buffer to hold the entire action line
  uint8_t actionLineIdx = 0;
  if (actionType >= ACTIONIDX_MAX) {
    logger.logPrint(A_INVALID_ACTION_TYPE, LOGAREA_SYSTEM, SEVERITY_ERROR);
    return;
  }
  memset(actionLineBuff, ' ', DISP_WIDTH);
  actionLineBuff[DISP_WIDTH] = '\0';
  #ifdef DISP_LCD
  lcd1.setCursor(0, ACTION_LINE_IDX);
  actionLineIdx = min((uint8_t)strlen(displayText[actionType]), (uint8_t)DISP_WIDTH);
  memcpy(actionLineBuff, displayText[actionType], actionLineIdx);
  #endif
  switch (actionType) {
    case ACTIONLINE_HOME:
      {
      #ifdef DISP_LCD
      std::time_t now = std::time(nullptr);
      std::tm* localTime = std::localtime(&now);
      char buffer[17];
      if (localTime == nullptr || std::strftime(buffer, sizeof(buffer), "%d.%m.%Y %H:%M", localTime) == 0) {
        strncpy(buffer, "--.--.---- --:--", sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';
      }
      lcd1.print (centerLine(buffer));
      break;
      }
      #endif
    case ACTIONLINE_OPEN_PASSWORD:
      for (uint8_t i = 0; i < currentPassLen && actionLineIdx < DISP_WIDTH; i++) {
        actionLineBuff[actionLineIdx++] = '*'; // Display stars for password input
      }
      #ifdef DISP_LCD
        lcd1.print(actionLineBuff); 
      #endif
      break;
    case ACTIONLINE_BADPASS:
      #ifdef DISP_LCD
        lcd1.print(actionLineBuff); 
      #endif
      break;
    case ACTIONLINE_CLOSE:
      #ifdef DISP_LCD
      lcd1.print(actionLineBuff); 
      #endif

      break;
    case ACTIONLINE_PRESENCE:
      for (uint8_t i = 0; currentVerifyCode[i] != '\0' && actionLineIdx < DISP_WIDTH; i++) {
        actionLineBuff[actionLineIdx++] = currentVerifyCode[i];
      }
      #ifdef DISP_LCD
        lcd1.print(actionLineBuff); 
      #endif
      break;
    default:
      if ( actionType < ACTIONIDX_MAX) {
        #ifdef DISP_LCD
          lcd1.print(actionLineBuff); 
        #endif
      } else {
        logger.logPrint(A_INVALID_ACTION_TYPE, LOGAREA_SYSTEM, SEVERITY_ERROR);
      }
      break;
  }
} // writeActionLine

void BoxDisplay::writeInfoLine (DisplayText infoType) {
    // Update the infoline as needed
    // Usualy used to display static helper text

  if (infoType >= RESPONSEIDX_MAX) {
  logger.logPrint(A_INVALID_INFO_TYPE, LOGAREA_SYSTEM, SEVERITY_ERROR);
  return;
  }
#ifdef DISP_LCD
    lcd1.setCursor(0, INFO_LINE_IDX);
    lcd1.print(displayText[infoType]);
#endif
} //writeInfoLine

void BoxDisplay::writeResponseLine (DisplayText responseType) {
  if (responseType >= RESPONSEIDX_MAX) {
    logger.logPrint(A_INVALID_RESPONSE_TYPE, LOGAREA_SYSTEM, SEVERITY_ERROR);
    return;
  }

  if (responseType == RESPONSELINE_PROGRESS) {
    char barBuf[PROGRESS_BAR_WIDTH + 1]; // Buffer to hold the progress bar string, adjust size as needed
    for (uint8_t i = 0; i < PROGRESS_BAR_WIDTH; i++) {
      if (i < (currentProgress * PROGRESS_BAR_WIDTH) / 100) {
        barBuf[i] = 'o'; // Filled part of the progress bar
      } else {  
        barBuf[i] = '.'; // Unfilled part of the progress bar
      }
    }
    barBuf[PROGRESS_BAR_WIDTH] = '\0'; // Null-terminate the string
    #ifdef DISP_LCD
      lcd1.setCursor(0, RESPONSE_LINE_IDX);
      lcd1.print(centerLine(barBuf)); 
    #endif
    return;
  } //writeResponseLine

 #ifdef DISP_LCD
    lcd1.setCursor(0, RESPONSE_LINE_IDX);
    lcd1.print(centerLine(displayText[responseType]));
#endif

}

void BoxDisplay::setCommunicationStatus (char comm_status) {
    currentStatus[STIDX_CLIENT] = comm_status;
}

void BoxDisplay::setLoggerStatus (char logger_status) {
    currentStatus[STIDX_LOGGER] = logger_status;
//    writeStatusLine();
}

void BoxDisplay::setLinkStatus (char link_status) {
    currentStatus[STIDX_LINK] = link_status;
    writeStatusLine();
}

void BoxDisplay::setOpenDoorList (void ) {
    // Update the currentDoorOpen array based on the provided list of open doors
    memset(currentDoorOpen, 0, sizeof(currentDoorOpen));
    bool doorBuf[DOOR_COUNT]; // Buffer to hold the door status string for display, adjust size as needed
    memset(doorBuf, 0, sizeof(doorBuf)); // Clear the buffer
    gpioHAL->getOpenDoors(doorBuf, DOOR_COUNT); // Get the current open doors from the GPIO HAL, fills the currentDoorOpen array with 1 for open and 0 for closed
    uint8_t idx = 0;
    uint8_t doorIdx = 0;
    while (idx < DOOR_COUNT && doorIdx + 1 < sizeof(currentDoorOpen)) {
        if (doorBuf[idx]) {
            uint8_t doorNum = idx + 1;
            currentDoorOpen[doorIdx++] = char((doorNum / 10) + '0');
            currentDoorOpen[doorIdx++] = char((doorNum % 10) + '0');
        }
        idx++;
    }
    currentDoorOpen[doorIdx] = '\0'; // Null-terminate the string
    writeStatusLine();
} //setOpenDoorList

void BoxDisplay::setPasswordLength (uint8_t pass_len) {
    currentPassLen = pass_len;
    writeActionLine(ACTIONLINE_OPEN_PASSWORD);
}

void BoxDisplay::setVerifyCode (const char* code) {
    strncpy(currentVerifyCode, code, PRESENCE_CODE_LENGTH); // Copy the provided code to the currentVerifyCode variable, ensuring it does not exceed the maximum length
    currentVerifyCode[PRESENCE_CODE_LENGTH] = '\0'; // Null-terminate the string
    writeActionLine(ACTIONLINE_PRESENCE);
}

// shows presence code expiration bar based on input 
void BoxDisplay::setProgressBar (uint8_t expirationProgress) {
    currentProgress = expirationProgress;
    writeResponseLine(RESPONSELINE_PROGRESS);
}

void BoxDisplay::setDisplayLanguage(int lang) {
  constexpr int DISPLAY_LANG_COUNT = sizeof(langDisplayText) / sizeof(langDisplayText[0]);
  if (lang < 0 || lang >= DISPLAY_LANG_COUNT) {
      lang = LANG_EN;   // fall back on bad/missing config
  }
  displayText = langDisplayText[lang];
}

void BoxDisplay::displayInit(GpioHAL* gpioHal) {
  gpioHAL = gpioHal; // Store the pointer to the GPIO HAL for later use in display updates
  displayText = langDisplayText[DISP_LANG];
#ifdef DISP_LCD
  logger.logPrint(A_DISPLAY_INITIALIZING, LOGAREA_SYSTEM, SEVERITY_INFO);
  lcd1.init();
  lcd1.backlight();
  lcd1.setCursor(0, 1);
  lcd1.println("OrangeBOX");
#endif
}

void BoxDisplay::writeProvisioning(const char* title, const char* action, const char* detail) {
#ifdef DISP_LCD
  const char* rows[] = {title, action, detail, ""};
  for (uint8_t row = 0; row < 4; ++row) {
    lcd1.setCursor(0, row);
    String line(rows[row]);
    line = line.substring(0, LCD_WIDTH);
    while (line.length() < LCD_WIDTH) line += ' ';
    lcd1.print(line.c_str());
  }
#endif
}

