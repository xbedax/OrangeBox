#ifndef CAMERA_H
#define CAMERA_H
#include "config.h"
#include <ESPAsyncWebServer.h>
#include <Arduino.h>

// Camera resolution
#define MAX_IMAGE_SIZE 30000

// Structure to maintain state for each streaming client
struct StreamState {
  uint8_t bufferIndex;  // Which buffer we're reading from
  uint32_t imageSize;   // Size of the image we're sending
  size_t offset;        // Current offset in the image
  bool headerSent;      // Whether we've sent the MIME header
  bool frameLocked;     // Whether we've locked this frame
};

// External camera globals (defined in camera.cpp)
extern uint8_t imageBuffer[2][MAX_IMAGE_SIZE];
extern volatile uint8_t writeBuffer;
extern volatile uint8_t readBuffer;
extern volatile uint32_t imageLength[2];
extern volatile bool imageReady[2];
extern SemaphoreHandle_t imageMutex;

#ifdef CAM_SPI
#include "Arducam_Mega.h"
extern Arducam_Mega myCAM;
#endif

// Callback function - called by captureThread() when data is available
uint8_t captureCallback(uint8_t *imagebuf, uint8_t length);

// Stop callback - called when preview should stop
void stopCallback();

// Function prototypes
void initCamera();
void handleStream(AsyncWebServerRequest *request);
void handleStream2(AsyncWebServerRequest *request);
void handleStream3(AsyncWebServerRequest *request);

#endif // CAMERA_H