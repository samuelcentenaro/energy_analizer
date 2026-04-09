#include "adc_hal.h"
#include "hal.h"
#include "hardware_config.h"

#include <esp_err.h>
#include <driver/gpio.h>
#include <soc/gpio_struct.h>

esp_err_t adc_hal_init(adc_continuous_handle_t *handle_out)
{
    if (handle_out != NULL) {
        *handle_out = NULL;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t adc_hal_start(adc_continuous_handle_t handle)
{
    (void)handle;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t adc_hal_stop(adc_continuous_handle_t handle)
{
    (void)handle;
    return ESP_OK;
}

esp_err_t adc_hal_deinit(adc_continuous_handle_t handle)
{
    (void)handle;
    return ESP_OK;
}

esp_err_t adc_hal_read_frame(adc_continuous_handle_t handle,
                             uint8_t *buffer,
                             size_t buffer_size,
                             size_t *out_bytes,
                             TickType_t timeout_ticks)
{
    (void)handle;
    (void)buffer;
    (void)buffer_size;
    if (out_bytes != NULL) {
        *out_bytes = 0U;
    }
    (void)timeout_ticks;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t board_buttons_init(void)
{
    const gpio_num_t button_pins[] = {
        BUTTON_PIN_UP,
        BUTTON_PIN_DOWN,
        BUTTON_PIN_SELECT
    };

    for (size_t index = 0U; index < (sizeof(button_pins) / sizeof(button_pins[0])); ++index) {
        gpio_config_t io_config = {
            .pin_bit_mask = (1ULL << (uint64_t)button_pins[index]),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };

        esp_err_t ret = gpio_config(&io_config);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

esp_err_t board_button_read(int gpio_num, bool *pressed)
{
    uint32_t level = 0U;

    if (pressed == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if ((gpio_num >= 0) && (gpio_num <= 31)) {
        level = (GPIO.in >> gpio_num) & 0x1U;
    } else if ((gpio_num >= 32) && (gpio_num <= 39)) {
        level = (GPIO.in1.val >> (gpio_num - 32)) & 0x1U;
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    *pressed = (level == 0U);
    return ESP_OK;
}
