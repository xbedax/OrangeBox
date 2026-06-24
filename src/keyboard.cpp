/*
Keyboard abstraction
the control routine should call handleKeyboard method regularly
Communicates via keyboardStates struct:
    - keyboardMode : either the password characters are collected, or pressed keys returned immediatelly
    - currentPasswordLen : number of password characters collected so far
    - passwordComplete : if password is non-empty and the # was pressed
    - noChange : true if no change ocurred - to quickly detect most frequent case
    - cancelPressed : if * pressed to exit current mode (i.e. when passlen = 0, during presence code etc.)
    - currentPassword password characters
    - currentKey - in non-password collection state the last key currently pressed

*/

#include <cstring>
#include <Wire.h>
#include "config.h"
#include "box_display.h"
#include "keyboard.h"
#include "state_machine.h"

extern BoxDisplay boxDisplay;

// Password completion
void BoxKeyboard::handleKey(uint8_t k, keyboardStatus *keyboardState) {
  if (k <= 9) {
    if (keyboardState->currentPasswordLen < PASS_MAX) {
      keyboardState->currentPassword[keyboardState->currentPasswordLen++] = '0' + k;
      boxDisplay.setPasswordLength(keyboardState->currentPasswordLen);
    }
    return;
  } 
  if (k == 0x1B) {  // '*' ... erase
    if (keyboardState->currentPasswordLen > 0) {
        keyboardState->currentPasswordLen--;
        boxDisplay.setPasswordLength(keyboardState->currentPasswordLen);
    } else {
//    keyboardState->currentPasswordLen = 0;   // CANCEL
//    keyboardState->passwordComplete = false;
//    keyboardState->keyboardMode = KEYBOARD_MODE_COMMAND;
      keyboardState->cancelPressed = true;

    }
    return;
  }
  if (k == 0x0D) {  // '#' ... enter
    if (keyboardState->currentPasswordLen > 0) {
      keyboardState->currentPassword[keyboardState->currentPasswordLen] = '\0';
      keyboardState->passwordComplete = true;
    }
    //### heslo je zadano, zpracovat - asi nejaky callback do mainu, ktery se postara o validaci a akci
    return;
  }
} // handleKey


// Polling wiegang convertor
bool BoxKeyboard::pollKeypad(keyboardStatus *keyboardState) {
  Wire.requestFrom(KEYPAD_I2C_ADDR, 32);  // víc než max buffer

  if (Wire.available() < 1)
    return false;

  uint8_t len = Wire.read();
  for (uint8_t i = 0; i < len && Wire.available(); i++) {
    uint8_t k = Wire.read();
//    Serial.print("PassLen: ");
//    Serial.print(pass_len);
//    Serial.print(" - ");
//    Serial.println(k);
    if (keyboardMode == KEYBOARD_MODE_COMMAND) {
        keyboardState->currentKey = k;
      // Handle command input if needed
    } else if (keyboardMode == KEYBOARD_MODE_PASSWORD) {
        handleKey(k, keyboardState);
    }  
  }
  return true;
}  // pollKeypad

/*  temer jiste smazat
// Password processing
bool BoxKeyboard::processPassword(keyboardStatus *keyboardState) {
  if (!keyboardState->passwordComplete)
    return;

  Serial.print("Heslo: ");
  for (uint8_t i = 0; i < keyboardState->currentPasswordLen; i++)
    Serial.print(keyboardState->currentPassword[i]);
  Serial.println();

    char passStr[15];         // Max IP string length is 15 chars + null terminator
    strcpy ( passStr, "Heslo:" );
    for (uint8_t i = 0; i < keyboardState->currentPasswordLen; i++){
      char passNum[4];
      sprintf(passNum, "%d", keyboardState->currentPassword[i]); 
      strcat ( passStr, passNum );
    }
//    boxDisplay->logPrint( passStr );
  } // processPassword
*/    

bool BoxKeyboard::pollKeyboard(keyboardStatus *keyboardState) {
    switch (keyboardState->keyboardMode ) {
        case KEYBOARD_MODE_COMMAND:
            if (keyboardMode != KEYBOARD_MODE_COMMAND) {    // mode switched to command, reset password input
              keyboardMode = KEYBOARD_MODE_COMMAND;
            }
            return (pollKeypad(keyboardState) );
        case KEYBOARD_MODE_PASSWORD:
            if (keyboardMode != KEYBOARD_MODE_PASSWORD) {
              keyboardMode = KEYBOARD_MODE_PASSWORD;
              keyboardState->currentPasswordLen = 0;
              keyboardState->passwordComplete = false;
            }
            return (pollKeypad(keyboardState));
        default:
            Serial.print("Invalid keyboard mode:" + keyboardState->keyboardMode);
            return false;

    }
} // pollKeyboard

void BoxKeyboard::handleKeyboard(keyboardStatus *keyboardState, BoxStateMachine *stateMachine) {
  if (keyboardState == nullptr || stateMachine == nullptr) {
    return;
  }

  BoxState state = stateMachine->getCurrentState();
  if (state == BoxState::Password) {
    keyboardState->keyboardMode = KEYBOARD_MODE_PASSWORD;
  } else {
    keyboardState->keyboardMode = KEYBOARD_MODE_COMMAND;
  }

  if (!pollKeyboard(keyboardState)) {
    return;
  }

  if (state == BoxState::Home && keyboardState->keyboardMode == KEYBOARD_MODE_COMMAND) {
    if (keyboardState->currentKey == KEYBOARD_KEY_1) {
      BoxEventData event = {};
      event.eventType = BoxEventType::KeyboardKey1;
      stateMachine->processEvent(event);
      keyboardState->currentKey = ' ';
      return;
    }

    if (keyboardState->currentKey == KEYBOARD_KEY_2) {
      BoxEventData event = {};
      event.eventType = BoxEventType::KeyboardKey2;
      stateMachine->processEvent(event);
      keyboardState->currentKey = ' ';
      return;
    }
  }

  if (state == BoxState::Password) {
    if (keyboardState->passwordComplete) {
      BoxEventData event = {};
      event.eventType = BoxEventType::KeyboardEnter;
      strncpy(event.data.keyboardData.password, keyboardState->currentPassword, sizeof(event.data.keyboardData.password) - 1);
      event.data.keyboardData.password[sizeof(event.data.keyboardData.password) - 1] = '\0';
      stateMachine->processEvent(event);

      keyboardState->passwordComplete = false;
      keyboardState->currentPasswordLen = 0;
      keyboardState->cancelPressed = false;
      clearPassword(keyboardState);
      return;
    }

    if (keyboardState->cancelPressed) {
      BoxEventData event = {};
      event.eventType = BoxEventType::KeyboardCancel;
      stateMachine->processEvent(event);
      keyboardState->cancelPressed = false;
      return;
    }
  }

  if (state == BoxState::BadPass || state == BoxState::Presence) {
    if (keyboardState->currentKey == KEYBOARD_KEY_CANCEL || keyboardState->cancelPressed) {
      BoxEventData event = {};
      event.eventType = BoxEventType::KeyboardCancel;
      stateMachine->processEvent(event);
      keyboardState->currentKey = ' ';
      keyboardState->cancelPressed = false;
      return;
    }
  }
  
}

void BoxKeyboard::clearPassword(keyboardStatus *keyboardState){
  for(int8_t i = 0; i < PASS_MAX; i++) {
    keyboardState->currentPassword[i] = ' ';
  }
}

void BoxKeyboard::keyboardInit(){
  return;
}
