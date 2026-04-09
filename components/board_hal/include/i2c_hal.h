#ifndef I2C_HAL_H
#define I2C_HAL_H

#include <hal/gpio_types.h>
#include <hal/i2c_types.h>
#include <esp_err.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t i2c_hal_init(i2c_port_t port,
                       gpio_num_t sda_pin,
                       gpio_num_t scl_pin,
                       uint32_t frequency_hz);

esp_err_t i2c_hal_deinit(i2c_port_t port);

esp_err_t i2c_hal_write_read(i2c_port_t port,
                             uint8_t address,
                             const uint8_t *write_data,
                             size_t write_len,
                             uint8_t *read_data,
                             size_t read_len);

esp_err_t i2c_hal_scan(i2c_port_t port,
                      uint8_t *found_addresses,
                      size_t max_devices,
                      size_t *found_count);

#ifdef __cplusplus
}
#endif

#endif // I2C_HAL_H
