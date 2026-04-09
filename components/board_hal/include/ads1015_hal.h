/**
 * @file ads1015_hal.h
 * @brief ADS1015 ADC Hardware Abstraction Layer
 *
 * This HAL provides a unified interface for the ADS1015 12-bit ADC,
 * abstracting I2C communication details and providing error handling.
 *
 * Features:
 * - Differential channel reading (A0-A1, A2-A3)
 * - Configurable PGA gain
 * - Single-shot conversion mode
 * - Thread-safe with mutex protection
 * - Error recovery and validation
 *
 * @note Requires I2C HAL to be initialized first
 * @note Uses ADS1015_I2C_ADDRESS from hardware_config.h
 */

#ifndef ADS1015_HAL_H
#define ADS1015_HAL_H

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Public Types
// ============================================================================

/// @brief ADS1015 channel configuration
typedef enum {
    ADS1015_CHANNEL_0_1 = 0,  ///< Differential A0-A1
    ADS1015_CHANNEL_2_3 = 1,  ///< Differential A2-A3
} ads1015_channel_t;

/// @brief ADS1015 PGA gain settings
typedef enum {
    ADS1015_GAIN_6_144V = 0,  ///< ±6.144V range
    ADS1015_GAIN_4_096V = 1,  ///< ±4.096V range
    ADS1015_GAIN_2_048V = 2,  ///< ±2.048V range (default)
    ADS1015_GAIN_1_024V = 3,  ///< ±1.024V range
    ADS1015_GAIN_0_512V = 4,  ///< ±0.512V range
    ADS1015_GAIN_0_256V = 5,  ///< ±0.256V range
} ads1015_gain_t;

/// @brief ADS1015 conversion result
typedef struct {
    int16_t raw_value;        ///< Raw 12-bit ADC value (-2048 to 2047)
    float voltage_mv;         ///< Converted voltage in millivolts
    bool data_ready;          ///< True if conversion completed successfully
} ads1015_result_t;

// ============================================================================
// Public API Functions
// ============================================================================

/**
 * @brief Initialize ADS1015 HAL
 *
 * Configures I2C communication and verifies device presence.
 * Must be called before any other ADS1015 functions.
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note Requires I2C HAL to be initialized first
 * @note Thread-safe on first call only
 */
esp_err_t ads1015_hal_init(void);

/**
 * @brief Deinitialize ADS1015 HAL
 *
 * Cleans up resources and resets device state.
 * Safe to call multiple times.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ads1015_hal_deinit(void);

/**
 * @brief Read differential voltage from specified channel
 *
 * Performs single-shot conversion on the specified differential channel
 * with configured PGA gain. Blocks until conversion completes.
 *
 * @param[in] channel Channel to read (ADS1015_CHANNEL_0_1 or ADS1015_CHANNEL_2_3)
 * @param[in] gain PGA gain setting for this conversion
 * @param[out] result Pointer to ads1015_result_t to fill with results
 * @return ESP_OK on success, error code otherwise
 *
 * @note Blocks for up to 1ms during conversion
 * @warning result must not be NULL
 */
esp_err_t ads1015_hal_read_channel(ads1015_channel_t channel,
                                  ads1015_gain_t gain,
                                  ads1015_result_t *result);

/**
 * @brief Get voltage range for PGA gain setting
 *
 * Returns the full-scale voltage range in millivolts for a given gain.
 *
 * @param[in] gain PGA gain setting
 * @return Full-scale range in millivolts (±value)
 */
float ads1015_hal_get_range_mv(ads1015_gain_t gain);

/**
 * @brief Convert raw ADC value to voltage
 *
 * Converts 12-bit signed ADC value to voltage in millivolts
 * based on the specified PGA gain.
 *
 * @param[in] raw_value Raw 12-bit ADC value (-2048 to 2047)
 * @param[in] gain PGA gain setting used for conversion
 * @return Voltage in millivolts
 */
float ads1015_hal_raw_to_voltage_mv(int16_t raw_value, ads1015_gain_t gain);

/**
 * @brief Test ADS1015 connectivity
 *
 * Performs basic connectivity test by reading device ID register.
 *
 * @return ESP_OK if device responds correctly, error code otherwise
 */
esp_err_t ads1015_hal_test_connection(void);

#ifdef __cplusplus
}
#endif

#endif // ADS1015_HAL_H
