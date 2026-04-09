/**
 * @file hal.h
 * @brief HAL layer - Hardware Abstraction
 * 
 * This layer abstracts hardware-specific details and provides
 * a unified interface to the services layer.
 * 
 * HAL layer components:
 * - adc_hal.h         → Legacy internal-ADC abstraction (inactive product path)
 * - i2c_hal.h         → I2C abstraction
 * - display_hal.h     → Display abstraction
 * - sensor_hal.h      → Sensor abstraction
 */

#ifndef HAL_H
#define HAL_H

#include "esp_err.h"
#include <stdbool.h>

// Include specific HAL functions as needed
#include "adc_hal.h"
#include "ads1015_hal.h"
#include "i2c_hal.h"
// #include "display_hal.h"
// #include "sensor_hal.h"

esp_err_t board_buttons_init(void);
esp_err_t board_button_read(int gpio_num, bool *pressed);

#endif // HAL_H
