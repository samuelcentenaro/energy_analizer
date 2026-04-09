#include "i2c_driver.h"

#include <driver/gpio.h>
#include <esp_log.h>

static const char *TAG = "I2C_DRIVER";

esp_err_t i2c_driver_init(i2c_port_t port, gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t frequency_hz)
{
    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = scl_pin,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = frequency_hz,
    };

    esp_err_t err = i2c_param_config(port, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: 0x%02x", err);
        return err;
    }

    err = i2c_driver_install(port, config.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: 0x%02x", err);
    }

    return err;
}

esp_err_t i2c_driver_deinit(i2c_port_t port)
{
    return i2c_driver_delete(port);
}

esp_err_t i2c_driver_scan(i2c_port_t port, uint8_t *found_addresses, size_t max_devices, size_t *found_count)
{
    if (found_addresses == NULL || found_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t count = 0;
    for (uint8_t address = 0; address < 0x80; ++address) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        if (cmd == NULL) {
            return ESP_ERR_NO_MEM;
        }

        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        esp_err_t err = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);

        if (err == ESP_OK) {
            if (count < max_devices) {
                found_addresses[count] = address;
            }
            ++count;
        } else if (err != ESP_ERR_TIMEOUT && err != ESP_FAIL) {
            return err;
        }
    }

    *found_count = count;
    return ESP_OK;
}

esp_err_t i2c_driver_write_read(i2c_port_t port, uint8_t address, const uint8_t *write_data, size_t write_len, uint8_t *read_data, size_t read_len)
{
    if ((write_len > 0 && write_data == NULL) || (read_len > 0 && read_data == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (write_len == 0 && read_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = ESP_OK;
    if (write_len > 0) {
        err = i2c_master_start(cmd);
    }
    if (err == ESP_OK && write_len > 0) {
        err = i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
    }
    if (err == ESP_OK && write_len > 0) {
        err = i2c_master_write(cmd, write_data, write_len, true);
    }

    if (read_len > 0) {
        err = i2c_master_start(cmd);
    }
    if (err == ESP_OK && read_len > 0) {
        err = i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, true);
    }
    if (err == ESP_OK && read_len > 1) {
        err = i2c_master_read(cmd, read_data, read_len - 1, I2C_MASTER_ACK);
    }
    if (err == ESP_OK && read_len > 0) {
        err = i2c_master_read_byte(cmd, read_data + read_len - 1, I2C_MASTER_NACK);
    }

    if (err == ESP_OK) {
        err = i2c_master_stop(cmd);
    }
    if (err == ESP_OK) {
        err = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(200));
    }

    i2c_cmd_link_delete(cmd);
    return err;
}
