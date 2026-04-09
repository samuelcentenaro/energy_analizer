/**
 * @file adc_driver.h
 * @brief Deprecated low-level driver for the legacy internal ADC path
 *
 * This driver is kept only for historical reference. The active production
 * firmware reads acquisition data from the external ADS1015 through
 * `components/board_hal/ads1015_hal.c`.
 */

#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#include <esp_err.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>

#if defined(__has_include)
#if __has_include(<esp_adc/adc_continuous.h>)
#include <esp_adc/adc_continuous.h>
#define ADC_DRIVER_CONTINUOUS_SUPPORTED 1
#elif __has_include(<driver/adc_continuous.h>)
#include <driver/adc_continuous.h>
#define ADC_DRIVER_CONTINUOUS_SUPPORTED 1
#else
typedef void *adc_continuous_handle_t;
#define ADC_DRIVER_CONTINUOUS_SUPPORTED 0
#endif
#else
typedef void *adc_continuous_handle_t;
#define ADC_DRIVER_CONTINUOUS_SUPPORTED 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the ADC continuous driver.
 *
 * @param[out] handle_out Pointer to receive the ADC driver handle.
 * @return ESP_OK on success, otherwise an ESP error.
 */
esp_err_t adc_driver_init(adc_continuous_handle_t *handle_out);

/**
 * @brief Deinitialize the ADC continuous driver.
 *
 * @param[in] handle ADC driver handle.
 * @return ESP_OK on success, otherwise an ESP error.
 */
esp_err_t adc_driver_deinit(adc_continuous_handle_t handle);

/**
 * @brief Start ADC continuous sampling.
 *
 * @param[in] handle ADC driver handle.
 * @return ESP_OK on success, otherwise an ESP error.
 */
esp_err_t adc_driver_start(adc_continuous_handle_t handle);

/**
 * @brief Stop ADC continuous sampling.
 *
 * @param[in] handle ADC driver handle.
 * @return ESP_OK on success, otherwise an ESP error.
 */
esp_err_t adc_driver_stop(adc_continuous_handle_t handle);

/**
 * @brief Read raw ADC conversion results from continuous mode.
 *
 * @param[in]  handle        ADC driver handle.
 * @param[out] buffer        Buffer to receive raw ADC frames.
 * @param[in]  buffer_size   Maximum number of bytes available.
 * @param[out] out_bytes     Actual number of bytes read.
 * @param[in]  timeout_ticks Timeout in FreeRTOS ticks.
 * @return ESP_OK on success, otherwise an ESP error.
 */
esp_err_t adc_driver_read(adc_continuous_handle_t handle,
                           uint8_t *buffer,
                           size_t buffer_size,
                           size_t *out_bytes,
                           TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif

#endif // ADC_DRIVER_H
