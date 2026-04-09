/**
 * @file adc_sensor.h
 * @brief Deprecated public API for the legacy internal ADC sensor component
 *
 * This header defines the public interface for the legacy internal ADC path
 * based on the ESP32 ADC peripheral.
 *
 * @note Deprecated: the production firmware uses the external ADS1015 path
 *       through `components/board_hal/ads1015_hal.c`.
 * @note This component remains in the repository only for reference and is
 *       not part of the active production data path.
 */

#ifndef ADC_SENSOR_H
#define ADC_SENSOR_H

#include "esp_err.h"
#include "common_types.h"

// ============================================================================
// Public API Functions
// ============================================================================

/**
 * @brief Initialize the ADC sensor component
 *
 * Configures ADC hardware, creates ring buffer, and sets up continuous
 * sampling at 8 kHz. Must be called before any other ADC functions.
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note This function is not thread-safe on first call
 * @warning ADC pins (GPIO34/35) must not be used elsewhere
 */
esp_err_t adc_sensor_init(void);

/**
 * @brief Deinitialize the ADC sensor component
 *
 * Stops sampling, frees resources, and cleans up hardware.
 * Safe to call multiple times.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t adc_sensor_deinit(void);

/**
 * @brief Get latest RMS measurements
 *
 * Returns the most recent RMS voltage and current calculations
 * based on the last 4000 samples (~500ms window).
 *
 * @param[out] measurement Pointer to rms_measurement_t structure to fill
 * @return ESP_OK on success, error code otherwise
 *
 * @note Blocks for up to ADC_READ_TIMEOUT_MS if data not ready
 * @warning measurement must not be NULL
 */
esp_err_t adc_sensor_read(rms_measurement_t *measurement);

/**
 * @brief Get current calibration parameters
 *
 * Retrieves the active calibration values used for ADC conversion.
 *
 * @param[out] calib Pointer to adc_calibration_t structure to fill
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t adc_sensor_get_calibration(adc_calibration_t *calib);

/**
 * @brief Set calibration parameters
 *
 * Updates the calibration values used for ADC conversion.
 * Changes take effect immediately.
 *
 * @param[in] calib Pointer to adc_calibration_t with new values
 * @return ESP_OK on success, error code otherwise
 *
 * @note Values are not persisted automatically - call save_calibration()
 */
esp_err_t adc_sensor_set_calibration(const adc_calibration_t *calib);

/**
 * @brief Save calibration to NVS flash
 *
 * Persists current calibration parameters to non-volatile storage.
 * Survives power cycles and firmware updates.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t adc_sensor_save_calibration(void);

/**
 * @brief Load calibration from NVS flash
 *
 * Restores calibration parameters from non-volatile storage.
 * Falls back to defaults if NVS data is invalid.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t adc_sensor_load_calibration(void);

/**
 * @brief Reset calibration to factory defaults
 *
 * Sets calibration to default values (offset=0, scale=1).
 * Does not save to NVS automatically.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t adc_sensor_reset_calibration(void);

/**
 * @brief Get diagnostic information
 *
 * Returns internal counters and status for debugging.
 *
 * @param[out] samples_processed Total ADC samples processed
 * @param[out] buffer_overflows Number of ring buffer overflows
 * @param[out] read_timeouts Number of read timeouts
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t adc_sensor_get_status(uint32_t *samples_processed,
                               uint32_t *buffer_overflows,
                               uint32_t *read_timeouts);

#endif // ADC_SENSOR_H
