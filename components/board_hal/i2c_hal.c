/**
 * @file i2c_hal.c
 * @brief I2C Hardware Abstraction Layer Implementation
 *
 * This implementation provides thread-safe I2C communication
 * with error handling and device scanning capabilities.
 *
 * Features:
 * - I2C master mode initialization
 * - Device scanning for bus discovery
 * - Write-read transactions
 * - Thread-safe with mutex protection
 * - Error recovery and validation
 *
 * @note Uses ESP-IDF I2C driver
 * @note Thread-safe for concurrent access
 */

#include <stdbool.h>
#include <string.h>
#include <driver/i2c.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <esp_check.h>

// Local includes
#include "i2c_hal.h"

static const char *TAG = "I2C_HAL";

// ============================================================================
// Internal Data Structures
// ============================================================================

/// @brief I2C HAL internal state
typedef struct {
    bool initialized;              ///< Initialization flag
    SemaphoreHandle_t mutex;       ///< Thread safety mutex
} i2c_hal_state_t;

static i2c_hal_state_t g_i2c_state[I2C_NUM_MAX] = {0};

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t i2c_hal_init(i2c_port_t port,
                       gpio_num_t sda_pin,
                       gpio_num_t scl_pin,
                       uint32_t frequency_hz)
{
    esp_err_t ret = ESP_OK;

    // Input validation
    if (port >= I2C_NUM_MAX) {
        ESP_LOGE(TAG, "Invalid I2C port: %d", port);
        return ESP_ERR_INVALID_ARG;
    }

    // Prevent re-initialization
    if (g_i2c_state[port].initialized) {
        ESP_LOGW(TAG, "I2C port %d already initialized", port);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing I2C HAL port %d (SDA=%d, SCL=%d, %d Hz)",
             port, sda_pin, scl_pin, frequency_hz);

    // Create mutex for thread safety
    g_i2c_state[port].mutex = xSemaphoreCreateMutex();
    if (g_i2c_state[port].mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex for port %d", port);
        return ESP_ERR_NO_MEM;
    }

    // Configure I2C
    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = scl_pin,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = frequency_hz,
        .clk_flags = 0
    };

    ret = i2c_param_config(port, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: 0x%02x", ret);
        vSemaphoreDelete(g_i2c_state[port].mutex);
        g_i2c_state[port].mutex = NULL;
        return ret;
    }

    // Install I2C driver
    ret = i2c_driver_install(port, config.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: 0x%02x", ret);
        vSemaphoreDelete(g_i2c_state[port].mutex);
        g_i2c_state[port].mutex = NULL;
        return ret;
    }

    g_i2c_state[port].initialized = true;
    ESP_LOGI(TAG, "I2C HAL port %d initialized successfully", port);

    return ESP_OK;
}

esp_err_t i2c_hal_deinit(i2c_port_t port)
{
    esp_err_t ret = ESP_OK;

    if (port >= I2C_NUM_MAX) {
        ESP_LOGE(TAG, "Invalid I2C port: %d", port);
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_i2c_state[port].initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing I2C HAL port %d", port);

    // Uninstall I2C driver
    ret = i2c_driver_delete(port);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver delete failed: 0x%02x", ret);
        // Continue with cleanup
    }

    // Clean up mutex
    if (g_i2c_state[port].mutex != NULL) {
        vSemaphoreDelete(g_i2c_state[port].mutex);
        g_i2c_state[port].mutex = NULL;
    }

    g_i2c_state[port].initialized = false;

    return ret;
}

esp_err_t i2c_hal_write_read(i2c_port_t port,
                             uint8_t address,
                             const uint8_t *write_data,
                             size_t write_len,
                             uint8_t *read_data,
                             size_t read_len)
{
    esp_err_t ret = ESP_OK;

    // Input validation
    if (port >= I2C_NUM_MAX) {
        ESP_LOGE(TAG, "Invalid I2C port: %d", port);
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_i2c_state[port].initialized) {
        ESP_LOGE(TAG, "I2C port %d not initialized", port);
        return ESP_ERR_INVALID_STATE;
    }

    if ((write_data == NULL && write_len > 0) ||
        (read_data == NULL && read_len > 0)) {
        ESP_LOGE(TAG, "Invalid buffer pointers");
        return ESP_ERR_INVALID_ARG;
    }

    // Take mutex
    if (xSemaphoreTake(g_i2c_state[port].mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex for port %d", port);
        return ESP_ERR_TIMEOUT;
    }

    // Perform I2C transaction
    if (write_len > 0 && read_len > 0) {
        // Write followed by read
        ret = i2c_master_write_read_device(port, address,
                                         write_data, write_len,
                                         read_data, read_len,
                                         pdMS_TO_TICKS(100));
    } else if (write_len > 0) {
        // Write only
        ret = i2c_master_write_to_device(port, address,
                                       write_data, write_len,
                                       pdMS_TO_TICKS(100));
    } else if (read_len > 0) {
        // Read only
        ret = i2c_master_read_from_device(port, address,
                                        read_data, read_len,
                                        pdMS_TO_TICKS(100));
    } else {
        // No data to transfer
        ret = ESP_ERR_INVALID_ARG;
    }

    // Release mutex
    xSemaphoreGive(g_i2c_state[port].mutex);

    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "I2C transaction failed: 0x%02x", ret);
    }

    return ret;
}

esp_err_t i2c_hal_scan(i2c_port_t port,
                      uint8_t *found_addresses,
                      size_t max_devices,
                      size_t *found_count)
{
    esp_err_t ret = ESP_OK;

    // Input validation
    if (port >= I2C_NUM_MAX) {
        ESP_LOGE(TAG, "Invalid I2C port: %d", port);
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_i2c_state[port].initialized) {
        ESP_LOGE(TAG, "I2C port %d not initialized", port);
        return ESP_ERR_INVALID_STATE;
    }

    if (found_addresses == NULL || found_count == NULL) {
        ESP_LOGE(TAG, "Invalid buffer pointers");
        return ESP_ERR_INVALID_ARG;
    }

    *found_count = 0;

    ESP_LOGI(TAG, "Scanning I2C bus on port %d", port);

    // Take mutex
    if (xSemaphoreTake(g_i2c_state[port].mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex for port %d", port);
        return ESP_ERR_TIMEOUT;
    }

    // Scan all possible I2C addresses (0x08 to 0x77)
    for (uint8_t addr = 0x08; addr < 0x78 && *found_count < max_devices; addr++) {
        // Skip reserved addresses
        if (addr >= 0x78 && addr <= 0x7F) continue;

        // Try to read one byte from the device
        uint8_t dummy;
        ret = i2c_master_read_from_device(port, addr, &dummy, 1, pdMS_TO_TICKS(10));

        if (ret == ESP_OK) {
            found_addresses[*found_count] = addr;
            (*found_count)++;
            ESP_LOGI(TAG, "Found I2C device at 0x%02x", addr);
        }
    }

    // Release mutex
    xSemaphoreGive(g_i2c_state[port].mutex);

    ESP_LOGI(TAG, "I2C scan complete, found %u device(s)", (unsigned)*found_count);

    return ESP_OK;
}
