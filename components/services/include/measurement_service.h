/**
 * @file measurement_service.h
 * @brief Measurement service for RMS and power calculations
 *
 * This service provides algorithms for calculating RMS voltage, current,
 * real power, apparent power, and power factor from raw ADC samples.
 */

#ifndef MEASUREMENT_SERVICE_H
#define MEASUREMENT_SERVICE_H

#include "esp_err.h"
#include "common_types.h"

#ifndef ESP_ERR_NOT_FINISHED
#define ESP_ERR_NOT_FINISHED ERROR_TO_ESP_ERR(0x10)
#endif

/**
 * @brief Initialize the measurement service
 *
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t measurement_service_init(void);

/**
 * @brief Process a single voltage/current sample pair
 *
 * This function accumulates samples and calculates RMS values
 * when enough samples are collected for the measurement window.
 *
 * @param voltage_raw Raw ADC voltage sample
 * @param current_raw Raw ADC current sample
 * @param result Pointer to store the measurement result
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for invalid parameters,
 *         ESP_ERR_NO_MEM if buffer is full
 */
esp_err_t measurement_service_process_samples(
    uint16_t voltage_raw,
    uint16_t current_raw,
    measurement_result_t *result);

/**
 * @brief Get active measurement calibration
 *
 * @param[out] calibration Pointer to the calibration structure
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t measurement_service_get_calibration(adc_calibration_t *calibration);

/**
 * @brief Set active measurement calibration
 *
 * @param[in] calibration Pointer to the calibration structure
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t measurement_service_set_calibration(const adc_calibration_t *calibration);

/**
 * @brief Get current field calibration record
 *
 * @param[out] calibration Pointer to the field calibration record
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t measurement_service_get_field_calibration(field_calibration_t *calibration);

/**
 * @brief Set voltage calibration reference from field measurement
 *
 * @param[in] reference_voltage_rms External reference RMS voltage
 * @param[in] measured_voltage_rms Measured RMS voltage before correction
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t measurement_service_set_voltage_reference(float reference_voltage_rms,
                                                    float measured_voltage_rms);

/**
 * @brief Set current calibration reference from field measurement
 *
 * @param[in] reference_current_rms External reference RMS current
 * @param[in] measured_current_rms Measured RMS current before correction
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t measurement_service_set_current_reference(float reference_current_rms,
                                                    float measured_current_rms);

/**
 * @brief Load persisted field calibration from NVS
 *
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t measurement_service_load_calibration(void);

/**
 * @brief Save active field calibration to NVS
 *
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t measurement_service_save_calibration(void);

/**
 * @brief Reset calibration to service defaults
 *
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t measurement_service_reset_calibration(void);

/**
 * @brief Get the current measurement statistics
 *
 * @param stats Pointer to store measurement statistics
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t measurement_service_get_stats(measurement_stats_t *stats);

/**
 * @brief Reset measurement accumulators
 *
 * Clears all accumulated samples and resets calculations.
 */
void measurement_service_reset(void);

#endif // MEASUREMENT_SERVICE_H
