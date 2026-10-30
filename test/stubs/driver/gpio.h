#pragma once
// ESP32-S3 GPIO capabilities for native configuration parser tests.
#define GPIO_IS_VALID_GPIO(pin) ((pin) <= 48 && ((pin) <= 21 || (pin) >= 26))
#define GPIO_IS_VALID_OUTPUT_GPIO(pin) (GPIO_IS_VALID_GPIO(pin) && (pin) != 46)
