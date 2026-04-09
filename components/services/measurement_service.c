/**
 * @file measurement_service.c
 * @brief Measurement service implementation
 */

#include "measurement_service.h"

#include <math.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "hardware_config.h"
#include "math_utils.h"

static const char *TAG = "MEASUREMENT_SVC";

#define SAMPLE_BUFFER_SIZE          100U
#define ADS1015_CENTER_CODE         2048.0f
#define VOLTAGE_FULL_SCALE_V        2.048f
#define CURRENT_FULL_SCALE_V        4.096f
#define CALIBRATION_MIN_SCALE       0.1f
#define CALIBRATION_MAX_SCALE       10.0f
#define CALIBRATION_MAX_OFFSET_ABS  500.0f
#define MEASUREMENT_NVS_NAMESPACE   "meas_cal"
#define NVS_KEY_VOLTAGE_OFFSET      "v_offset"
#define NVS_KEY_VOLTAGE_SCALE       "v_scale"
#define NVS_KEY_CURRENT_OFFSET      "c_offset"
#define NVS_KEY_CURRENT_SCALE       "c_scale"

static bool g_initialized = false;
static measurement_stats_t g_stats = {0};
static field_calibration_t g_field_calibration = {
    .coefficients = {
        .voltage_offset = 0.0f,
        .voltage_scale = 1.0f,
        .current_offset = 0.0f,
        .current_scale = 1.0f
    },
    .reference = {0},
    .state = CALIBRATION_STATE_FACTORY_DEFAULT,
    .updated_at_ms = 0U,
    .has_voltage_reference = false,
    .has_current_reference = false
};
static adc_calibration_t g_calibration = {
    .voltage_offset = 0.0f,
    .voltage_scale = 1.0f,
    .current_offset = 0.0f,
    .current_scale = 1.0f
};
static SemaphoreHandle_t g_mutex = NULL;
static float g_voltage_samples[SAMPLE_BUFFER_SIZE];
static float g_current_samples[SAMPLE_BUFFER_SIZE];
static uint32_t g_sample_index = 0U;

static float measurement_convert_raw_to_ads_voltage(uint16_t raw, float full_scale_voltage);
static float measurement_convert_raw_to_line_voltage(uint16_t raw);
static float measurement_convert_raw_to_line_current(uint16_t raw);
static float measurement_calculate_real_power(const float *voltage_samples,
                                              const float *current_samples,
                                              uint32_t count);
static esp_err_t measurement_validate_calibration(const adc_calibration_t *calibration);
static esp_err_t measurement_open_nvs(nvs_handle_t *handle, nvs_open_mode_t mode);
static void measurement_update_stats(const measurement_result_t *result);
static void measurement_reset_buffers(void);
static void measurement_sync_field_calibration(void);

static float measurement_convert_raw_to_ads_voltage(uint16_t raw, float full_scale_voltage)
{
    float signed_code = (float)raw - ADS1015_CENTER_CODE;

    return (signed_code / ADS1015_CENTER_CODE) * full_scale_voltage;
}

static float measurement_convert_raw_to_line_voltage(uint16_t raw)
{
    float adc_voltage = measurement_convert_raw_to_ads_voltage(raw, VOLTAGE_FULL_SCALE_V);
    float line_voltage = (adc_voltage / VOLTAGE_FULL_SCALE_V) *
                         VOLTAGE_SENSOR_RANGE_V *
                         VOLTAGE_DIVIDER_RATIO;

    return (line_voltage * g_calibration.voltage_scale) + g_calibration.voltage_offset;
}

static float measurement_convert_raw_to_line_current(uint16_t raw)
{
    float adc_voltage = measurement_convert_raw_to_ads_voltage(raw, CURRENT_FULL_SCALE_V);
    float line_current = (adc_voltage / CURRENT_FULL_SCALE_V) * CURRENT_SENSOR_RANGE_A;

    return (line_current * g_calibration.current_scale) + g_calibration.current_offset;
}

static float measurement_calculate_real_power(const float *voltage_samples,
                                              const float *current_samples,
                                              uint32_t count)
{
    double sum = 0.0;
    uint32_t index;

    if ((voltage_samples == NULL) || (current_samples == NULL) || (count == 0U)) {
        return 0.0f;
    }

    for (index = 0U; index < count; ++index) {
        sum += (double)voltage_samples[index] * (double)current_samples[index];
    }

    return (float)(sum / (double)count);
}

static esp_err_t measurement_validate_calibration(const adc_calibration_t *calibration)
{
    if (calibration == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if ((calibration->voltage_scale < CALIBRATION_MIN_SCALE) ||
        (calibration->voltage_scale > CALIBRATION_MAX_SCALE)) {
        return ESP_ERR_INVALID_ARG;
    }

    if ((calibration->current_scale < CALIBRATION_MIN_SCALE) ||
        (calibration->current_scale > CALIBRATION_MAX_SCALE)) {
        return ESP_ERR_INVALID_ARG;
    }

    if ((fabsf(calibration->voltage_offset) > CALIBRATION_MAX_OFFSET_ABS) ||
        (fabsf(calibration->current_offset) > CALIBRATION_MAX_OFFSET_ABS)) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t measurement_open_nvs(nvs_handle_t *handle, nvs_open_mode_t mode)
{
    esp_err_t ret;

    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = nvs_flash_init();
    if ((ret != ESP_OK) && (ret != ESP_ERR_NVS_NO_FREE_PAGES) && (ret != ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        return ret;
    }

    if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) || (ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            return ret;
        }

        ret = nvs_flash_init();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return nvs_open(MEASUREMENT_NVS_NAMESPACE, mode, handle);
}

static void measurement_update_stats(const measurement_result_t *result)
{
    if (result == NULL) {
        return;
    }

    g_stats.total_samples += result->sample_count;
    g_stats.valid_measurements++;

    if (g_stats.valid_measurements == 1U) {
        g_stats.avg_voltage = result->voltage_rms;
        g_stats.avg_current = result->current_rms;
        return;
    }

    g_stats.avg_voltage += (result->voltage_rms - g_stats.avg_voltage) /
                           (float)g_stats.valid_measurements;
    g_stats.avg_current += (result->current_rms - g_stats.avg_current) /
                           (float)g_stats.valid_measurements;
}

static void measurement_reset_buffers(void)
{
    memset(g_voltage_samples, 0, sizeof(g_voltage_samples));
    memset(g_current_samples, 0, sizeof(g_current_samples));
    g_sample_index = 0U;
}

static void measurement_sync_field_calibration(void)
{
    g_field_calibration.coefficients = g_calibration;

    if (g_field_calibration.has_voltage_reference || g_field_calibration.has_current_reference) {
        g_field_calibration.state = CALIBRATION_STATE_APPLIED;
    } else {
        g_field_calibration.state = CALIBRATION_STATE_FACTORY_DEFAULT;
    }

    g_field_calibration.updated_at_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

esp_err_t measurement_service_init(void)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "Measurement service already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing measurement service...");

    g_mutex = xSemaphoreCreateMutex();
    if (g_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    memset(&g_stats, 0, sizeof(g_stats));
    measurement_reset_buffers();
    measurement_sync_field_calibration();

    g_initialized = true;
    ESP_LOGI(TAG, "Measurement service initialized");
    ESP_LOGI(TAG, "Calibration defaults: V(offset=%.3f scale=%.3f) I(offset=%.3f scale=%.3f)",
             g_calibration.voltage_offset,
             g_calibration.voltage_scale,
             g_calibration.current_offset,
             g_calibration.current_scale);

    (void)measurement_service_load_calibration();
    return ESP_OK;
}

esp_err_t measurement_service_process_samples(uint16_t voltage_raw,
                                              uint16_t current_raw,
                                              measurement_result_t *result)
{
    float voltage_sample;
    float current_sample;
    float apparent_power;
    float real_power;
    float reactive_power;

    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100U)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    voltage_sample = measurement_convert_raw_to_line_voltage(voltage_raw);
    current_sample = measurement_convert_raw_to_line_current(current_raw);

    if (g_sample_index < SAMPLE_BUFFER_SIZE) {
        g_voltage_samples[g_sample_index] = voltage_sample;
        g_current_samples[g_sample_index] = current_sample;
        g_sample_index++;
    }

    if (g_sample_index < SAMPLE_BUFFER_SIZE) {
        xSemaphoreGive(g_mutex);
        return ESP_ERR_NOT_FINISHED;
    }

    memset(result, 0, sizeof(*result));

    result->voltage_rms = math_rms(g_voltage_samples, SAMPLE_BUFFER_SIZE);
    result->current_rms = math_rms(g_current_samples, SAMPLE_BUFFER_SIZE);

    apparent_power = result->voltage_rms * result->current_rms;
    real_power = measurement_calculate_real_power(g_voltage_samples,
                                                  g_current_samples,
                                                  SAMPLE_BUFFER_SIZE);

    if (real_power < 0.0f) {
        real_power = -real_power;
    }

    reactive_power = (apparent_power * apparent_power) - (real_power * real_power);
    if (reactive_power < 0.0f) {
        reactive_power = 0.0f;
    }

    result->power_real = real_power;
    result->power_apparent = apparent_power;
    result->power_reactive = sqrtf(reactive_power);
    result->power_factor = (apparent_power > 0.0f) ? (real_power / apparent_power) : 0.0f;
    result->power_factor = math_constrain(result->power_factor, 0.0f, 1.0f);
    result->sample_count = SAMPLE_BUFFER_SIZE;
    result->timestamp_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    measurement_update_stats(result);
    measurement_reset_buffers();

    xSemaphoreGive(g_mutex);
    return ESP_OK;
}

esp_err_t measurement_service_get_calibration(adc_calibration_t *calibration)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (calibration == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100U)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memcpy(calibration, &g_calibration, sizeof(*calibration));

    xSemaphoreGive(g_mutex);
    return ESP_OK;
}

esp_err_t measurement_service_set_calibration(const adc_calibration_t *calibration)
{
    esp_err_t ret;

    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ret = measurement_validate_calibration(calibration);
    if (ret != ESP_OK) {
        return ret;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100U)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memcpy(&g_calibration, calibration, sizeof(g_calibration));
    measurement_sync_field_calibration();

    xSemaphoreGive(g_mutex);

    ESP_LOGI(TAG, "Calibration updated: V(offset=%.3f scale=%.3f) I(offset=%.3f scale=%.3f)",
             g_calibration.voltage_offset,
             g_calibration.voltage_scale,
             g_calibration.current_offset,
             g_calibration.current_scale);
    return ESP_OK;
}

esp_err_t measurement_service_get_field_calibration(field_calibration_t *calibration)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (calibration == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100U)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memcpy(calibration, &g_field_calibration, sizeof(*calibration));

    xSemaphoreGive(g_mutex);
    return ESP_OK;
}

esp_err_t measurement_service_set_voltage_reference(float reference_voltage_rms,
                                                    float measured_voltage_rms)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if ((reference_voltage_rms <= 0.0f) || (measured_voltage_rms <= 0.0f)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100U)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    g_field_calibration.reference.reference_voltage_rms = reference_voltage_rms;
    g_field_calibration.reference.measured_voltage_rms = measured_voltage_rms;
    g_field_calibration.has_voltage_reference = true;
    g_field_calibration.state = CALIBRATION_STATE_VALIDATED;

    g_calibration.voltage_scale = reference_voltage_rms / measured_voltage_rms;
    g_field_calibration.coefficients = g_calibration;
    g_field_calibration.updated_at_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    xSemaphoreGive(g_mutex);

    ESP_LOGI(TAG, "Voltage field calibration set: ref=%.3f measured=%.3f scale=%.3f",
             reference_voltage_rms,
             measured_voltage_rms,
             g_calibration.voltage_scale);
    return ESP_OK;
}

esp_err_t measurement_service_set_current_reference(float reference_current_rms,
                                                    float measured_current_rms)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if ((reference_current_rms <= 0.0f) || (measured_current_rms <= 0.0f)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100U)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    g_field_calibration.reference.reference_current_rms = reference_current_rms;
    g_field_calibration.reference.measured_current_rms = measured_current_rms;
    g_field_calibration.has_current_reference = true;
    g_field_calibration.state = CALIBRATION_STATE_VALIDATED;

    g_calibration.current_scale = reference_current_rms / measured_current_rms;
    g_field_calibration.coefficients = g_calibration;
    g_field_calibration.updated_at_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    xSemaphoreGive(g_mutex);

    ESP_LOGI(TAG, "Current field calibration set: ref=%.3f measured=%.3f scale=%.3f",
             reference_current_rms,
             measured_current_rms,
             g_calibration.current_scale);
    return ESP_OK;
}

esp_err_t measurement_service_get_stats(measurement_stats_t *stats)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100U)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memcpy(stats, &g_stats, sizeof(*stats));

    xSemaphoreGive(g_mutex);
    return ESP_OK;
}

esp_err_t measurement_service_load_calibration(void)
{
    adc_calibration_t temp = {
        .voltage_offset = 0.0f,
        .voltage_scale = 1.0f,
        .current_offset = 0.0f,
        .current_scale = 1.0f
    };
    nvs_handle_t handle = 0;
    size_t size = sizeof(float);
    esp_err_t ret;

    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ret = measurement_open_nvs(&handle, NVS_READONLY);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Calibration NVS unavailable, using defaults: 0x%02x", ret);
        return ret;
    }

    if (nvs_get_blob(handle, NVS_KEY_VOLTAGE_OFFSET, &temp.voltage_offset, &size) != ESP_OK) {
        temp.voltage_offset = 0.0f;
    }
    size = sizeof(float);
    if (nvs_get_blob(handle, NVS_KEY_VOLTAGE_SCALE, &temp.voltage_scale, &size) != ESP_OK) {
        temp.voltage_scale = 1.0f;
    }
    size = sizeof(float);
    if (nvs_get_blob(handle, NVS_KEY_CURRENT_OFFSET, &temp.current_offset, &size) != ESP_OK) {
        temp.current_offset = 0.0f;
    }
    size = sizeof(float);
    if (nvs_get_blob(handle, NVS_KEY_CURRENT_SCALE, &temp.current_scale, &size) != ESP_OK) {
        temp.current_scale = 1.0f;
    }

    nvs_close(handle);

    ret = measurement_validate_calibration(&temp);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Persisted calibration invalid, resetting to defaults");
        return measurement_service_reset_calibration();
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100U)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    g_calibration = temp;
    measurement_sync_field_calibration();

    xSemaphoreGive(g_mutex);

    ESP_LOGI(TAG, "Calibration loaded: V(offset=%.3f scale=%.3f) I(offset=%.3f scale=%.3f)",
             g_calibration.voltage_offset,
             g_calibration.voltage_scale,
             g_calibration.current_offset,
             g_calibration.current_scale);
    return ESP_OK;
}

esp_err_t measurement_service_save_calibration(void)
{
    nvs_handle_t handle = 0;
    esp_err_t ret;

    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ret = measurement_open_nvs(&handle, NVS_READWRITE);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_set_blob(handle, NVS_KEY_VOLTAGE_OFFSET, &g_calibration.voltage_offset, sizeof(float));
    if (ret == ESP_OK) {
        ret = nvs_set_blob(handle, NVS_KEY_VOLTAGE_SCALE, &g_calibration.voltage_scale, sizeof(float));
    }
    if (ret == ESP_OK) {
        ret = nvs_set_blob(handle, NVS_KEY_CURRENT_OFFSET, &g_calibration.current_offset, sizeof(float));
    }
    if (ret == ESP_OK) {
        ret = nvs_set_blob(handle, NVS_KEY_CURRENT_SCALE, &g_calibration.current_scale, sizeof(float));
    }
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }

    nvs_close(handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration saved to NVS");
    }

    return ret;
}

esp_err_t measurement_service_reset_calibration(void)
{
    adc_calibration_t defaults = {
        .voltage_offset = 0.0f,
        .voltage_scale = 1.0f,
        .current_offset = 0.0f,
        .current_scale = 1.0f
    };
    esp_err_t ret;

    ret = measurement_service_set_calibration(&defaults);
    if (ret != ESP_OK) {
        return ret;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100U)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memset(&g_field_calibration.reference, 0, sizeof(g_field_calibration.reference));
    g_field_calibration.has_voltage_reference = false;
    g_field_calibration.has_current_reference = false;
    measurement_sync_field_calibration();

    xSemaphoreGive(g_mutex);

    return measurement_service_save_calibration();
}

void measurement_service_reset(void)
{
    if (!g_initialized) {
        return;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100U)) != pdTRUE) {
        return;
    }

    memset(&g_stats, 0, sizeof(g_stats));
    measurement_reset_buffers();

    xSemaphoreGive(g_mutex);
}
