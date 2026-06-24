#include <Arduino.h>
#include "config.h"


#define KEYPAD_I2C_ADDR 0x42 // I2C address of the keypad controller, adjust as needed

#define KEYBOARD_MODE_COMMAND      1
#define KEYBOARD_MODE_PASSWORD     2

#define KEYBOARD_KEY_1              1
#define KEYBOARD_KEY_2              2
#define KEYBOARD_KEY_ENTER          0x0D        // #
#define KEYBOARD_KEY_CANCEL         0x1B        // *



typedef struct {
    uint8_t keyboardMode;
    uint8_t currentPasswordLen;
    bool passwordComplete;
//    bool noChange;
    bool cancelPressed;
    char currentPassword[PASS_MAX+1];
    char currentKey;
} keyboardStatus;

class BoxStateMachine;

class BoxKeyboard {
public:
    void keyboardInit();
    void handleKeyboard(keyboardStatus *keyboardState, BoxStateMachine *stateMachine);
    void clearPassword(keyboardStatus *keyboardState);
    

private:
    
//    uint8_t pass[PASS_MAX];
//    uint8_t pass_len = 0;
//    bool passComplete = false;
//    bool keyboardChange = false;
    uint8_t keyboardMode    = 0;
//    BoxDisplay* boxDisplay; // Pointer to the BoxDisplay instance for updating the display based on keyboard input
//    bool pass_ready = false;

    void handleKey(uint8_t k, keyboardStatus *keyboardState);   
    bool pollKeypad(keyboardStatus *keyboardState);
    bool pollKeyboard(keyboardStatus *keyboardState);
    bool processPassword(keyboardStatus *keyboardState);
//    void handleCommand(uint8_t key);
};

