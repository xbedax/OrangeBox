#include <Arduino.h>
#include "esp_chip_info.h"
#include "esp_flash.h"

void setup()
{
    Serial.begin(9600);
    delay(5000);

    esp_chip_info_t chip;
    esp_chip_info(&chip);

    uint32_t flash_size = 0;
    esp_flash_get_size(nullptr, &flash_size);

    Serial.printf("Chip rev: %d\n", chip.revision);
    Serial.printf("Cores: %d\n", chip.cores);

    Serial.printf("Flash: %.2f MiB\n",
        flash_size / 1048576.0);

    Serial.printf("PSRAM found: %s\n",
        psramFound() ? "YES" : "NO");

    Serial.printf("PSRAM size: %.2f MiB\n",
        ESP.getPsramSize() / 1048576.0);

    Serial.printf("PSRAM free: %.2f MiB\n",
        ESP.getFreePsram() / 1048576.0);
}

void loop() {}
