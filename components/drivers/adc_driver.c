#include "adc_driver.h"

#include <esp_log.h>
#include "hardware_config.h"
#include "timing_config.h"

static const char *TAG = "ADC_DRIVER";

#define ADC_DRIVER_MAX_STORE_BUF_SIZE 4096
#define ADC_DRIVER_CONV_FRAME_SIZE   1024
#define ADC_DRIVER_READ_BUFFER_SIZE  2048
#define ADC_DRIVER_MAX_DELAY         UINT32_MAX

static uint32_t adc_driver_ticks_to_ms(TickType_t ticks)
{
    if (ticks == portMAX_DELAY) {
        return ADC_DRIVER_MAX_DELAY;
    }
    return (uint32_t)(ticks * portTICK_PERIOD_MS);
}

esp_err_t adc_driver_init(adc_continuous_handle_t *handle_out)
{
    if (handle_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#if ADC_DRIVER_CONTINUOUS_SUPPORTED
    adc_continuous_handle_cfg_t handle_cfg = {
        .max_store_buf_size = ADC_DRIVER_MAX_STORE_BUF_SIZE,
        .conv_frame_size = ADC_DRIVER_CONV_FRAME_SIZE,
        .flags = {
            .flush_pool = 1,
        },
    };

    esp_err_t err = adc_continuous_new_handle(&handle_cfg, handle_out);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_continuous_new_handle failed: 0x%02x", err);
        return err;
    }

    adc_digi_pattern_config_t pattern[2] = {
        {
            .atten = ADC_ATTEN,
            .channel = ADC_CHANNEL_VOLTAGE,
            .unit = ADC_UNIT,
            .bit_width = ADC_BITWIDTH,
        },
        {
            .atten = ADC_ATTEN,
            .channel = ADC_CHANNEL_CURRENT,
            .unit = ADC_UNIT,
            .bit_width = ADC_BITWIDTH,
        },
    };

    adc_continuous_config_t config = {
        .pattern_num = 2,
        .adc_pattern = pattern,
        .sample_freq_hz = ADC_SAMPLING_FREQUENCY_HZ,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };

    err = adc_continuous_config(*handle_out, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_continuous_config failed: 0x%02x", err);
        adc_continuous_deinit(*handle_out);
        *handle_out = NULL;
    }

    return err;
#else
    *handle_out = NULL;
    ESP_LOGW(TAG, "ADC continuous API not available in this ESP-IDF");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t adc_driver_deinit(adc_continuous_handle_t handle)
{
#if ADC_DRIVER_CONTINUOUS_SUPPORTED
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return adc_continuous_deinit(handle);
#else
    (void)handle;
    return ESP_OK;
#endif
}

esp_err_t adc_driver_start(adc_continuous_handle_t handle)
{
#if ADC_DRIVER_CONTINUOUS_SUPPORTED
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return adc_continuous_start(handle);
#else
    (void)handle;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t adc_driver_stop(adc_continuous_handle_t handle)
{
#if ADC_DRIVER_CONTINUOUS_SUPPORTED
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return adc_continuous_stop(handle);
#else
    (void)handle;
    return ESP_OK;
#endif
}

esp_err_t adc_driver_read(adc_continuous_handle_t handle,
                           uint8_t *buffer,
                           size_t buffer_size,
                           size_t *out_bytes,
                           TickType_t timeout_ticks)
{
    if (handle == NULL || buffer == NULL || out_bytes == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#if ADC_DRIVER_CONTINUOUS_SUPPORTED
    uint32_t out_length = 0;
    uint32_t timeout_ms = adc_driver_ticks_to_ms(timeout_ticks);
    esp_err_t err = adc_continuous_read(handle,
                                        buffer,
                                        (uint32_t)buffer_size,
                                        &out_length,
                                        timeout_ms);
    *out_bytes = (size_t)out_length;
    return err;
#else
    (void)handle;
    (void)buffer;
    (void)buffer_size;
    (void)timeout_ticks;
    *out_bytes = 0U;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
