/**
 * @file ads1015_hal.c
 * @brief ADS1015 ADC Hardware Abstraction Layer Implementation
 *
 * This implementation provides thread-safe access to the ADS1015 12-bit ADC
 * with proper error handling, I2C communication, and conversion calculations.
 *
 * Key features:
 * - I2C communication with error recovery
 * - Single-shot conversion mode
 * - PGA gain configuration
 * - Thread-safe with FreeRTOS mutex
 * - Input validation and error checking
 *
 * CRITICAL MAINTENANCE NOTES:
 * - I2C transactions must complete within timeout to prevent blocking
 * - PGA gain affects conversion range - verify against sensor output
 * - Differential channels require proper sensor wiring
 * - Device address must match hardware configuration
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <esp_check.h>

// Local includes
#include "ads1015_hal.h"
#include "i2c_hal.h"
#include "hardware_config.h"
#include "timing_config.h"

static const char *TAG = "ADS1015_HAL";

// ============================================================================
// ADS1015 Register Definitions
// ============================================================================

#define ADS1015_REG_CONVERSION    0x00  ///< Conversion result register
#define ADS1015_REG_CONFIG        0x01  ///< Configuration register
#define ADS1015_REG_LO_THRESH     0x02  ///< Low threshold register
#define ADS1015_REG_HI_THRESH     0x03  ///< High threshold register

// ============================================================================
// ADS1015 Configuration Register Bits
// ============================================================================

#define ADS1015_CONFIG_OS_MASK    0x8000  ///< Operational status
#define ADS1015_CONFIG_OS_START   0x8000  ///< Start single conversion
#define ADS1015_CONFIG_OS_READY   0x8000  ///< Conversion ready

#define ADS1015_CONFIG_MUX_MASK   0x7000  ///< Input multiplexer
#define ADS1015_CONFIG_MUX_0_1    0x0000  ///< A0-A1 differential
#define ADS1015_CONFIG_MUX_2_3    0x3000  ///< A2-A3 differential

#define ADS1015_CONFIG_PGA_MASK   0x0E00  ///< PGA gain
#define ADS1015_CONFIG_PGA_6_144V 0x0000  ///< ±6.144V
#define ADS1015_CONFIG_PGA_4_096V 0x0200  ///< ±4.096V
#define ADS1015_CONFIG_PGA_2_048V 0x0400  ///< ±2.048V
#define ADS1015_CONFIG_PGA_1_024V 0x0600  ///< ±1.024V
#define ADS1015_CONFIG_PGA_0_512V 0x0800  ///< ±0.512V
#define ADS1015_CONFIG_PGA_0_256V 0x0A00  ///< ±0.256V

#define ADS1015_CONFIG_MODE_MASK  0x0100  ///< Conversion mode
#define ADS1015_CONFIG_MODE_CONT  0x0000  ///< Continuous conversion
#define ADS1015_CONFIG_MODE_SINGLE 0x0100 ///< Single-shot conversion

#define ADS1015_CONFIG_DR_MASK    0x00E0  ///< Data rate
#define ADS1015_CONFIG_DR_128SPS  0x0000  ///< 128 samples/second
#define ADS1015_CONFIG_DR_250SPS  0x0020  ///< 250 samples/second
#define ADS1015_CONFIG_DR_490SPS  0x0040  ///< 490 samples/second
#define ADS1015_CONFIG_DR_920SPS  0x0060  ///< 920 samples/second
#define ADS1015_CONFIG_DR_1600SPS 0x0080  ///< 1600 samples/second (default)
#define ADS1015_CONFIG_DR_2400SPS 0x00A0  ///< 2400 samples/second
#define ADS1015_CONFIG_DR_3300SPS 0x00C0  ///< 3300 samples/second

#define ADS1015_CONFIG_COMP_MODE_MASK  0x0010  ///< Comparator mode
#define ADS1015_CONFIG_COMP_MODE_TRAD  0x0000  ///< Traditional comparator
#define ADS1015_CONFIG_COMP_MODE_WINDOW 0x0010 ///< Window comparator

#define ADS1015_CONFIG_COMP_POL_MASK   0x0008  ///< Comparator polarity
#define ADS1015_CONFIG_COMP_POL_LOW    0x0000  ///< Active low
#define ADS1015_CONFIG_COMP_POL_HIGH   0x0008  ///< Active high

#define ADS1015_CONFIG_COMP_LAT_MASK   0x0004  ///< Comparator latching
#define ADS1015_CONFIG_COMP_LAT_OFF    0x0000  ///< Non-latching
#define ADS1015_CONFIG_COMP_LAT_ON     0x0004  ///< Latching

#define ADS1015_CONFIG_COMP_QUE_MASK   0x0003  ///< Comparator queue
#define ADS1015_CONFIG_COMP_QUE_1      0x0000  ///< Assert after 1 conversion
#define ADS1015_CONFIG_COMP_QUE_2      0x0001  ///< Assert after 2 conversions
#define ADS1015_CONFIG_COMP_QUE_4      0x0002  ///< Assert after 4 conversions
#define ADS1015_CONFIG_COMP_QUE_OFF    0x0003  ///< Disable comparator

#define ADS1015_CONVERSION_WAIT_MS     10U

// ============================================================================
// Internal Data Structures
// ============================================================================

/// @brief ADS1015 HAL internal state
typedef struct {
    bool initialized;              ///< Initialization flag
    SemaphoreHandle_t mutex;       ///< Thread safety mutex
    i2c_port_t i2c_port;           ///< I2C port number
} ads1015_hal_state_t;

static ads1015_hal_state_t g_ads1015_state = {
    .initialized = false,
    .mutex = NULL,
    .i2c_port = I2C_NUM_0
};

// ============================================================================
// Internal Helper Functions
// ============================================================================

/**
 * @brief Write 16-bit register value to ADS1015
 */
static esp_err_t ads1015_write_register(uint8_t reg, uint16_t value)
{
    uint8_t data[3];
    data[0] = reg;
    data[1] = (value >> 8) & 0xFF;
    data[2] = value & 0xFF;

    return i2c_hal_write_read(g_ads1015_state.i2c_port,
                             ADS1015_I2C_ADDRESS,
                             data, sizeof(data),
                             NULL, 0);
}

/**
 * @brief Read 16-bit register value from ADS1015
 */
static esp_err_t ads1015_read_register(uint8_t reg, uint16_t *value)
{
    uint8_t data[2];
    esp_err_t ret;

    ret = i2c_hal_write_read(g_ads1015_state.i2c_port,
                            ADS1015_I2C_ADDRESS,
                            &reg, 1,
                            data, sizeof(data));
    if (ret != ESP_OK) {
        return ret;
    }

    *value = (data[0] << 8) | data[1];
    return ESP_OK;
}

/**
 * @brief Get PGA gain configuration bits
 */
static uint16_t ads1015_get_pga_config(ads1015_gain_t gain)
{
    switch (gain) {
        case ADS1015_GAIN_6_144V: return ADS1015_CONFIG_PGA_6_144V;
        case ADS1015_GAIN_4_096V: return ADS1015_CONFIG_PGA_4_096V;
        case ADS1015_GAIN_2_048V: return ADS1015_CONFIG_PGA_2_048V;
        case ADS1015_GAIN_1_024V: return ADS1015_CONFIG_PGA_1_024V;
        case ADS1015_GAIN_0_512V: return ADS1015_CONFIG_PGA_0_512V;
        case ADS1015_GAIN_0_256V: return ADS1015_CONFIG_PGA_0_256V;
        default: return ADS1015_CONFIG_PGA_2_048V; // Default to ±2.048V
    }
}

/**
 * @brief Get channel multiplexer configuration bits
 */
static uint16_t ads1015_get_mux_config(ads1015_channel_t channel)
{
    switch (channel) {
        case ADS1015_CHANNEL_0_1: return ADS1015_CONFIG_MUX_0_1;
        case ADS1015_CHANNEL_2_3: return ADS1015_CONFIG_MUX_2_3;
        default: return ADS1015_CONFIG_MUX_0_1;
    }
}

/**
 * @brief Get full-scale voltage range for PGA gain
 */
static float ads1015_get_full_scale_mv(ads1015_gain_t gain)
{
    switch (gain) {
        case ADS1015_GAIN_6_144V: return 6144.0f;
        case ADS1015_GAIN_4_096V: return 4096.0f;
        case ADS1015_GAIN_2_048V: return 2048.0f;
        case ADS1015_GAIN_1_024V: return 1024.0f;
        case ADS1015_GAIN_0_512V: return 512.0f;
        case ADS1015_GAIN_0_256V: return 256.0f;
        default: return 2048.0f;
    }
}

static bool ads1015_is_valid_channel(ads1015_channel_t channel)
{
    return (channel == ADS1015_CHANNEL_0_1) || (channel == ADS1015_CHANNEL_2_3);
}

static bool ads1015_is_valid_gain(ads1015_gain_t gain)
{
    return (gain >= ADS1015_GAIN_6_144V) && (gain <= ADS1015_GAIN_0_256V);
}

static esp_err_t ads1015_wait_for_conversion(void)
{
    uint32_t elapsed_ms = 0U;

    while (elapsed_ms < ADS1015_CONVERSION_WAIT_MS) {
        uint16_t config_value = 0U;
        esp_err_t ret = ads1015_read_register(ADS1015_REG_CONFIG, &config_value);

        if (ret != ESP_OK) {
            return ret;
        }

        if ((config_value & ADS1015_CONFIG_OS_READY) != 0U) {
            return ESP_OK;
        }

        vTaskDelay(pdMS_TO_TICKS(1U));
        ++elapsed_ms;
    }

    return ESP_ERR_TIMEOUT;
}

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t ads1015_hal_init(void)
{
    esp_err_t ret = ESP_OK;

    // Prevent re-initialization
    if (g_ads1015_state.initialized) {
        ESP_LOGW(TAG, "ADS1015 HAL already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing ADS1015 HAL");

    // Create mutex for thread safety
    g_ads1015_state.mutex = xSemaphoreCreateMutex();
    if (g_ads1015_state.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    // Test device connectivity
    ret = ads1015_hal_test_connection();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADS1015 connectivity test failed: 0x%02x", ret);
        vSemaphoreDelete(g_ads1015_state.mutex);
        g_ads1015_state.mutex = NULL;
        return ret;
    }

    g_ads1015_state.initialized = true;
    ESP_LOGI(TAG, "ADS1015 HAL initialized successfully");

    return ESP_OK;
}

esp_err_t ads1015_hal_deinit(void)
{
    if (!g_ads1015_state.initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing ADS1015 HAL");

    // Clean up mutex
    if (g_ads1015_state.mutex != NULL) {
        vSemaphoreDelete(g_ads1015_state.mutex);
        g_ads1015_state.mutex = NULL;
    }

    g_ads1015_state.initialized = false;

    return ESP_OK;
}

esp_err_t ads1015_hal_read_channel(ads1015_channel_t channel,
                                  ads1015_gain_t gain,
                                  ads1015_result_t *result)
{
    esp_err_t ret = ESP_OK;
    uint16_t config = 0;
    uint16_t conversion = 0;
    int16_t raw_value = 0;

    // Input validation
    if (result == NULL) {
        ESP_LOGE(TAG, "Invalid result pointer");
        return ESP_ERR_INVALID_ARG;
    }

    (void)memset(result, 0, sizeof(*result));

    if (!ads1015_is_valid_channel(channel) || !ads1015_is_valid_gain(gain)) {
        ESP_LOGE(TAG, "Invalid ADS1015 channel/gain configuration");
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_ads1015_state.initialized) {
        ESP_LOGE(TAG, "ADS1015 HAL not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Take mutex
    if (xSemaphoreTake(g_ads1015_state.mutex, pdMS_TO_TICKS(SYNC_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex");
        return ESP_ERR_TIMEOUT;
    }

    // Build configuration register value
    config = ADS1015_CONFIG_OS_START |           // Start conversion
             ads1015_get_mux_config(channel) |   // Channel selection
             ads1015_get_pga_config(gain) |      // PGA gain
             ADS1015_CONFIG_MODE_SINGLE |        // Single-shot mode
             ADS1015_CONFIG_DR_1600SPS |         // 1600 SPS
             ADS1015_CONFIG_COMP_QUE_OFF;        // Disable comparator

    // Write configuration to start conversion
    ret = ads1015_write_register(ADS1015_REG_CONFIG, config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write config register: 0x%02x", ret);
        xSemaphoreGive(g_ads1015_state.mutex);
        return ret;
    }

    ret = ads1015_wait_for_conversion();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADS1015 conversion timeout/error: 0x%02x", ret);
        xSemaphoreGive(g_ads1015_state.mutex);
        return ret;
    }

    // Read conversion result
    ret = ads1015_read_register(ADS1015_REG_CONVERSION, &conversion);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read conversion register: 0x%02x", ret);
        xSemaphoreGive(g_ads1015_state.mutex);
        return ret;
    }

    // Convert to signed 12-bit value
    raw_value = (int16_t)(conversion >> 4);  // Shift right to get 12-bit value
    if (raw_value & 0x0800) {                 // Check if negative (sign extend)
        raw_value |= 0xF000;
    }

    // Fill result structure
    result->raw_value = raw_value;
    result->voltage_mv = ads1015_hal_raw_to_voltage_mv(raw_value, gain);
    result->data_ready = true;

    // Release mutex
    xSemaphoreGive(g_ads1015_state.mutex);

    ESP_LOGD(TAG, "Channel %d: raw=%d, voltage=%.2f mV",
             channel, raw_value, result->voltage_mv);

    return ESP_OK;
}

float ads1015_hal_get_range_mv(ads1015_gain_t gain)
{
    return ads1015_get_full_scale_mv(gain);
}

float ads1015_hal_raw_to_voltage_mv(int16_t raw_value, ads1015_gain_t gain)
{
    float full_scale_mv = ads1015_get_full_scale_mv(gain);
    // 12-bit ADC: 2048 = full scale, so scale factor is full_scale_mv / 2048
    return (float)raw_value * full_scale_mv / 2048.0f;
}

esp_err_t ads1015_hal_test_connection(void)
{
    uint16_t config = 0;
    esp_err_t ret;

    // Try to read configuration register
    ret = ads1015_read_register(ADS1015_REG_CONFIG, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read config register: 0x%02x", ret);
        return ret;
    }

    ESP_LOGI(TAG, "ADS1015 connection test successful, config=0x%04x", config);
    return ESP_OK;
}
