#ifndef ADC_HAL_H
#define ADC_HAL_H

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <stddef.h>

#if defined(__has_include)
#if __has_include(<esp_adc/adc_continuous.h>)
#include <esp_adc/adc_continuous.h>
#elif __has_include(<driver/adc_continuous.h>)
#include <driver/adc_continuous.h>
#else
typedef void *adc_continuous_handle_t;
#endif
#else
typedef void *adc_continuous_handle_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t adc_hal_init(adc_continuous_handle_t *handle_out);
esp_err_t adc_hal_start(adc_continuous_handle_t handle);
esp_err_t adc_hal_stop(adc_continuous_handle_t handle);
esp_err_t adc_hal_deinit(adc_continuous_handle_t handle);
esp_err_t adc_hal_read_frame(adc_continuous_handle_t handle,
                             uint8_t *buffer,
                             size_t buffer_size,
                             size_t *out_bytes,
                             TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif

#endif // ADC_HAL_H
