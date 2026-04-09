/**
 * @file drivers.h
 * @brief Drivers layer umbrella header
 *
 * This layer provides low-level hardware access with minimal abstraction.
 * The production firmware keeps the official acquisition path in
 * `board_hal/ads1015_hal.c` and uses the driver layer only where direct
 * ESP-IDF peripheral wrappers are still needed.
 *
 * Active drivers:
 * - i2c_driver.h
 *
 * Legacy note:
 * - `adc_driver.h` is intentionally not re-exported here. It belongs to the
 *   deprecated internal-ADC path kept only for repository history.
 */

#ifndef DRIVERS_H
#define DRIVERS_H

#include "esp_err.h"

#include "i2c_driver.h"

#endif // DRIVERS_H
