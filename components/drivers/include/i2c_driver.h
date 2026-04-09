#ifndef I2C_DRIVER_H
#define I2C_DRIVER_H

#include <driver/gpio.h>
#include <driver/i2c.h>
#include <esp_err.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t i2c_driver_init(i2c_port_t port, gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t frequency_hz);
esp_err_t i2c_driver_deinit(i2c_port_t port);
esp_err_t i2c_driver_scan(i2c_port_t port, uint8_t *found_addresses, size_t max_devices, size_t *found_count);
esp_err_t i2c_driver_write_read(i2c_port_t port, uint8_t address, const uint8_t *write_data, size_t write_len, uint8_t *read_data, size_t read_len);

#ifdef __cplusplus
}
#endif

#endif // I2C_DRIVER_H
