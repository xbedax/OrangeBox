#include "config.h"
#include "camera.h"
#include <Arduino.h>

#ifdef CAM_SPI
#include "Arducam_Mega.h"
#endif

// Camera object
const int CS = 7;                               // Camera SPI CS pin
Arducam_Mega myCAM(CS);

// Image buffer - 30KB should be enough for 320x240 JPEG
uint8_t imageBuffer[2][MAX_IMAGE_SIZE];
volatile uint8_t writeBuffer = 0;
volatile uint8_t readBuffer = 0;
volatile uint32_t imageLength[2] = { 0, 0 };
volatile uint32_t bytesWritten = 0;
volatile bool imageReady[2] = { false, false };
volatile bool jpegStarted = false;


// Synchronization
SemaphoreHandle_t imageMutex;

// Callback function - called by captureThread() when data is available
uint8_t captureCallback(uint8_t *imagebuf, uint8_t length) {
  // Check for JPEG header (0xFF 0xD8)
  //    Serial.printf("Callback running: %u\n", length);
  if (imagebuf[0] == 0xFF && imagebuf[1] == 0xD8) {
    //        Serial.printf("%u", writeBuffer);
    jpegStarted = true;
    bytesWritten = 0;
    imageLength[writeBuffer] = 0;
    //        imageReady[writeBuffer] = false;
  }

  // Copy data to buffer if we've started and have room
  if (jpegStarted && (bytesWritten + length) < MAX_IMAGE_SIZE) {
    //            Serial.print("Chunk writting - ");
    //        if (xSemaphoreTake(imageMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    memcpy(&imageBuffer[writeBuffer][bytesWritten], imagebuf, length);
    bytesWritten += length;
    imageLength[writeBuffer] = bytesWritten;
    //            xSemaphoreGive(imageMutex);
    //            Serial.println("Chunk written.");
    //        }
  }

  // Check if we've received the complete image
  if (bytesWritten >= myCAM.getTotalLength()) {
    while (xSemaphoreTake(imageMutex, pdMS_TO_TICKS(10)) != pdTRUE) {
      delay(5);
    }
    //        Serial.println ("JPEG finished");
    delay(2);
    jpegStarted = false;
    imageReady[writeBuffer] = true;
    readBuffer = writeBuffer;
    writeBuffer = (writeBuffer + 1) % 2;
    xSemaphoreGive(imageMutex);
  }
  //    Serial.println ("Callback finished");
  return 1;  // Continue capture
}

// Stop callback - called when preview should stop
void stopCallback() {
  Serial.println("Preview stopped");
}

void initCamera() {
  imageMutex = xSemaphoreCreateMutex();
  myCAM.reset();
  delay(100);
  myCAM.begin();
  myCAM.registerCallBack(captureCallback, 200, stopCallback);
} // initCamera

// ============================================
// WebServer Endpoint Handlers
// ============================================

void handleStream(AsyncWebServerRequest *request) {
  Serial.println("Stream client connected");

  // Allocate state for this client (will be freed when connection closes)
  auto state = std::make_shared<StreamState>();
  state->offset = 0;
  state->headerSent = false;
  state->frameLocked = false;
  state->bufferIndex = 255;  // Invalid initially
  state->imageSize = 0;

  AsyncWebServerResponse *response = request->beginChunkedResponse(
  "multipart/x-mixed-replace; boundary=frame",
  [state](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
      // If we finished the previous frame, prepare for next one
      if (!state->frameLocked || state->offset >= state->imageSize) {
      // Wait for a new frame to be ready
          if (xSemaphoreTake(imageMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
              uint8_t currentRead = readBuffer;

              if (imageReady[currentRead]) {
              // Lock onto this buffer for the entire frame
              state->bufferIndex = currentRead;
              state->imageSize = imageLength[currentRead];
              state->offset = 0;
              state->headerSent = false;
              state->frameLocked = true;

              xSemaphoreGive(imageMutex);
              } else {
              xSemaphoreGive(imageMutex);
              delay(10);
              return 0;  // No new frame yet
              }
          } else {
              delay(5);
              return 0;  // Couldn't get mutex
          }
      }
      size_t bytesToWrite = 0;
      // Send MIME header if not sent yet
      if (!state->headerSent) {
      String header = "--frame\r\n"
                      "Content-Type: image/jpeg\r\n"
                      "Content-Length: "
                      + String(state->imageSize) + "\r\n\r\n";

      size_t headerLen = header.length();
      if (headerLen > maxLen) {
          Serial.println("ERROR: Header too large for buffer!");
          return 0;
      }

      memcpy(buffer, header.c_str(), headerLen);
      bytesToWrite = headerLen;
      state->headerSent = true;

      } else if (state->offset < state->imageSize) {
      // Send image data
      size_t remaining = state->imageSize - state->offset;
      size_t toSend = min(remaining, maxLen);

      memcpy(buffer, imageBuffer[state->bufferIndex] + state->offset, toSend);
      state->offset += toSend;
      bytesToWrite = toSend;

      // If we finished this frame, add the boundary footer
      if (state->offset >= state->imageSize) {
          if (bytesToWrite + 2 <= maxLen) {
          buffer[bytesToWrite++] = '\r';
          buffer[bytesToWrite++] = '\n';
          state->frameLocked = false;  // Ready for next frame
          Serial.printf("Frame complete: %u bytes\n", state->imageSize);
          }
      }
      }

      return bytesToWrite;
  });
  request->send(response);
} // handleStream

void handleStream2(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginChunkedResponse(
    "multipart/x-mixed-replace; boundary=frame",
    [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
      // Wait for image to be ready
      if (imageReady[readBuffer] && xSemaphoreTake(imageMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Build multipart frame header
        String header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + String(imageLength[readBuffer]) + "\r\n\r\n";

        size_t headerLen = header.length();
        size_t footerLen = 2;  // "\r\n"
        size_t totalSize = headerLen + imageLength[readBuffer] + footerLen;

        if (index == 0 && maxLen >= totalSize) {
          // Send complete frame
          memcpy(buffer, header.c_str(), headerLen);
          memcpy(buffer + headerLen, imageBuffer[readBuffer], imageLength[readBuffer]);
          buffer[headerLen + imageLength[readBuffer]] = '\r';
          buffer[headerLen + imageLength[readBuffer] + 1] = '\n';

          imageReady[readBuffer] = false;
          xSemaphoreGive(imageMutex);

          return totalSize;
        }

        xSemaphoreGive(imageMutex);
      }

      // No data ready yet
      delay(10);
      return 0;
    });
  request->send(response);
} // handleStream2

void handleStream3(AsyncWebServerRequest *request) {
  Serial.println("Stream requested");
  AsyncWebServerResponse *response = request->beginChunkedResponse(
    "multipart/x-mixed-replace; boundary=frame",
    [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
      // Wait for a new image to be ready
      if (!imageReady[readBuffer]) {
        delay(10);
        return 0;  // Buffer sent already
      }
      // Try to acquire the image
      while (xSemaphoreTake(imageMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        delay(5);  // Couldn't get mutex, try again
      }
      static size_t frameOffset = 0;
      static bool sendingHeader = true;
      static uint32_t imgLen = imageLength[readBuffer];
      if (index == 0) {
        frameOffset = 0;
        sendingHeader = true;
      }
      size_t bytesWritten = 0;
      if (sendingHeader) {
        String header = "--frame\r\n";
        header += "Content-Type: image/jpeg\r\n";
        header += "Content-Length: " + String(imgLen) + "\r\n\r\n";

        size_t headerLen = header.length();
        if (headerLen <= maxLen) {
          memcpy(buffer, header.c_str(), headerLen);
          bytesWritten = headerLen;
          sendingHeader = false;
        } else {
          Serial.println("Header too long!");
        }
      } else {
        size_t remaining = imgLen - frameOffset;
        size_t toSend = min(remaining, maxLen);

        memcpy(buffer, imageBuffer[readBuffer] + frameOffset, toSend);
        frameOffset += toSend;
        bytesWritten = toSend;

        if (frameOffset >= imgLen) {
          frameOffset = 0;
          sendingHeader = true;
        }
      }
      xSemaphoreGive(imageMutex);
      Serial.printf("Chunk served: %d bytes\n", bytesWritten);
      return bytesWritten;
    });
  request->send(response);
} // handleStream3
