/**
 * @file adc_sensor.c
 * @brief ADC sensor component implementation
 *
 * This file implements continuous ADC sampling for voltage and current
 * measurement using ESP32 ADC in continuous mode with DMA.
 *
 * Key features:
 * - 8 kHz sampling rate (125µs per sample)
 * - Ring buffer for 4000 samples (~500ms RMS window)
 * - Real-time RMS calculation
 * - Thread-safe with FreeRTOS mutex
 * - Calibration persistence in NVS
 * - ISR callback for high-performance data acquisition
 *
 * CRITICAL MAINTENANCE NOTES:
 * - ISR runs at 16000 calls/second (2 channels) - must complete in <100µs
 * - Ring buffer prevents data loss during processing delays
 * - Calibration affects all measurements - test after changes
 * - ADC continuous mode requires careful resource management
 */

#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "nvs.h"

// Local includes
#include "adc_sensor.h"
#include "adc_driver.h"
#include "hardware_config.h"
#include "timing_config.h"
#include "common_types.h"
#include "feature_config.h"

static const char *TAG = "ADC_SENSOR";

#if defined(__has_include)
#if __has_include(<esp_adc/adc_continuous.h>)
#include <esp_adc/adc_continuous.h>
#define ADC_SENSOR_CONTINUOUS_SUPPORTED 1
#elif __has_include(<driver/adc_continuous.h>)
#include <driver/adc_continuous.h>
#define ADC_SENSOR_CONTINUOUS_SUPPORTED 1
#else
#define ADC_SENSOR_CONTINUOUS_SUPPORTED 0
#endif
#else
#define ADC_SENSOR_CONTINUOUS_SUPPORTED 0
#endif

#if !ADC_SENSOR_CONTINUOUS_SUPPORTED

esp_err_t adc_sensor_init(void)
{
    ESP_LOGW(TAG, "ADC continuous API not available in this ESP-IDF");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t adc_sensor_deinit(void)
{
    return ESP_OK;
}

esp_err_t adc_sensor_read(rms_measurement_t *measurement)
{
    (void)measurement;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t adc_sensor_get_calibration(adc_calibration_t *calib)
{
    (void)calib;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t adc_sensor_set_calibration(const adc_calibration_t *calib)
{
    (void)calib;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t adc_sensor_save_calibration(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t adc_sensor_load_calibration(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t adc_sensor_reset_calibration(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t adc_sensor_get_status(uint32_t *samples_processed,
                                uint32_t *buffer_overflows,
                                uint32_t *read_timeouts)
{
    (void)samples_processed;
    (void)buffer_overflows;
    (void)read_timeouts;
    return ESP_ERR_NOT_SUPPORTED;
}

#else

// ============================================================================
// Internal Data Structures
// ============================================================================

/// @brief Ring buffer for ADC samples
typedef struct {
    uint16_t voltage[ADC_RMS_WINDOW_SAMPLES];  ///< Voltage samples
    uint16_t current[ADC_RMS_WINDOW_SAMPLES];  ///< Current samples
    uint32_t head;                             ///< Write position
    uint32_t count;                            ///< Valid samples count
} adc_ring_buffer_t;

/// @brief ADC sensor internal state
typedef struct {
    adc_continuous_handle_t handle;            ///< ESP-IDF ADC handle
    adc_ring_buffer_t buffer;                  ///< Sample ring buffer
    adc_calibration_t calibration;             ///< Active calibration
    SemaphoreHandle_t mutex;                   ///< Thread safety
    TaskHandle_t task_handle;                  ///< Processing task
    volatile bool initialized;                 ///< Init flag
    volatile bool running;                     ///< Sampling active flag

    // Diagnostic counters
    uint32_t samples_processed;
    uint32_t buffer_overflows;
    uint32_t read_timeouts;
} adc_sensor_state_t;

// ============================================================================
// Global State (Static to this file)
// ============================================================================

static adc_sensor_state_t g_adc_state = {0};

// ============================================================================
// NVS Keys for Calibration Persistence
// ============================================================================

#define NVS_NAMESPACE "adc_cal"
#define NVS_KEY_VOLTAGE_OFFSET "v_offset"
#define NVS_KEY_VOLTAGE_SCALE  "v_scale"
#define NVS_KEY_CURRENT_OFFSET "c_offset"
#define NVS_KEY_CURRENT_SCALE  "c_scale"

// ============================================================================
// Internal Function Prototypes
// ============================================================================

static bool adc_isr_callback(adc_continuous_handle_t handle,
                           const adc_continuous_evt_data_t *edata,
                           void *user_data);
static void adc_processing_task(void *arg);
static esp_err_t adc_configure_hardware(void);
static esp_err_t adc_start_sampling(void);
static esp_err_t adc_stop_sampling(void);
static void adc_process_raw_data(const uint8_t *data, size_t size);
static float adc_calculate_rms(const uint16_t *samples, uint32_t count);
static esp_err_t adc_validate_calibration(const adc_calibration_t *calib);

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t adc_sensor_init(void)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "Initializing ADC sensor at %d Hz...",
             ADC_SAMPLING_FREQUENCY_HZ);

    // Check if already initialized
    if (g_adc_state.initialized) {
        ESP_LOGW(TAG, "ADC sensor already initialized");
        return ESP_OK;
    }

    // Initialize state
    memset(&g_adc_state, 0, sizeof(adc_sensor_state_t));
    g_adc_state.running = false;

    // Load calibration from NVS (ignore errors, use defaults)
    adc_sensor_load_calibration();

    // Create mutex for thread safety
    g_adc_state.mutex = xSemaphoreCreateMutex();
    if (g_adc_state.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    // Configure ADC hardware
    ret = adc_configure_hardware();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hardware configuration failed: 0x%02x", ret);
        vSemaphoreDelete(g_adc_state.mutex);
        return ret;
    }

    // Start sampling
    ret = adc_start_sampling();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start sampling: 0x%02x", ret);
        vSemaphoreDelete(g_adc_state.mutex);
        return ret;
    }

    // Create processing task
    BaseType_t task_ret = xTaskCreate(
        adc_processing_task,
        "adc_proc",
        ADC_TASK_STACK_SIZE,
        NULL,
        ADC_TASK_PRIORITY,
        &g_adc_state.task_handle
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create processing task");
        adc_stop_sampling();
        vSemaphoreDelete(g_adc_state.mutex);
        return ESP_ERR_NO_MEM;
    }

    // Mark as initialized
    g_adc_state.initialized = true;

    ESP_LOGI(TAG, "ADC sensor initialized successfully!");
    ESP_LOGI(TAG, "  Sampling rate: %d Hz", ADC_SAMPLING_FREQUENCY_HZ);
    ESP_LOGI(TAG, "  RMS window: %d samples (~%d ms)",
             ADC_RMS_WINDOW_SAMPLES,
             (ADC_RMS_WINDOW_SAMPLES * 1000) / ADC_SAMPLING_FREQUENCY_HZ);
    ESP_LOGI(TAG, "  Voltage: offset=%.3f mV, scale=%.6f",
             g_adc_state.calibration.voltage_offset,
             g_adc_state.calibration.voltage_scale);
    ESP_LOGI(TAG, "  Current: offset=%.3f mA, scale=%.6f",
             g_adc_state.calibration.current_offset,
             g_adc_state.calibration.current_scale);

    return ESP_OK;
}

esp_err_t adc_sensor_deinit(void)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "Deinitializing ADC sensor...");

    if (!g_adc_state.initialized) {
        ESP_LOGW(TAG, "ADC sensor not initialized");
        return ESP_OK;
    }

    // Stop sampling first
    ret = adc_stop_sampling();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Error stopping sampling: 0x%02x", ret);
    }

    // Delete processing task
    if (g_adc_state.task_handle != NULL) {
        vTaskDelete(g_adc_state.task_handle);
        g_adc_state.task_handle = NULL;
    }

    // Deinitialize ADC driver handle
    if (g_adc_state.handle != NULL) {
        adc_driver_deinit(g_adc_state.handle);
        g_adc_state.handle = NULL;
    }

    // Delete mutex
    if (g_adc_state.mutex != NULL) {
        vSemaphoreDelete(g_adc_state.mutex);
        g_adc_state.mutex = NULL;
    }

    // Reset state
    memset(&g_adc_state, 0, sizeof(adc_sensor_state_t));

    ESP_LOGI(TAG, "ADC sensor deinitialized");
    return ESP_OK;
}

esp_err_t adc_sensor_read(rms_measurement_t *measurement)
{
    if (measurement == NULL) {
        ESP_LOGE(TAG, "Null pointer provided");
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_adc_state.initialized) {
        ESP_LOGE(TAG, "ADC sensor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Take mutex
    if (!xSemaphoreTake(g_adc_state.mutex, pdMS_TO_TICKS(SYNC_TIMEOUT_MS))) {
        ESP_LOGE(TAG, "Mutex timeout");
        return ESP_ERR_TIMEOUT;
    }

    // Check if we have enough samples
    if (g_adc_state.buffer.count < ADC_RMS_WINDOW_SAMPLES) {
        xSemaphoreGive(g_adc_state.mutex);
        ESP_LOGW(TAG, "Insufficient samples: %d/%d",
                 g_adc_state.buffer.count, ADC_RMS_WINDOW_SAMPLES);
        g_adc_state.read_timeouts++;
        return ESP_ERR_TIMEOUT;
    }

    // Calculate RMS values
    measurement->voltage_rms = adc_calculate_rms(
        g_adc_state.buffer.voltage, ADC_RMS_WINDOW_SAMPLES);
    measurement->current_rms = adc_calculate_rms(
        g_adc_state.buffer.current, ADC_RMS_WINDOW_SAMPLES);

    // Apply calibration
    measurement->voltage_rms = (measurement->voltage_rms *
                               g_adc_state.calibration.voltage_scale) +
                               g_adc_state.calibration.voltage_offset;
    measurement->current_rms = (measurement->current_rms *
                               g_adc_state.calibration.current_scale) +
                               g_adc_state.calibration.current_offset;

    // Convert to engineering units (assuming 3.3V ADC range)
    const float adc_max_voltage = 3.3f;
    const float adc_max_count = 4095.0f;

    measurement->voltage_rms *= (adc_max_voltage / adc_max_count) *
                               VOLTAGE_SENSOR_RANGE_V;
    measurement->current_rms *= (adc_max_voltage / adc_max_count) *
                               CURRENT_SENSOR_RANGE_A / CURRENT_BURDEN_RESISTOR;

    measurement->timestamp_s = (float)xTaskGetTickCount() / configTICK_RATE_HZ;

    xSemaphoreGive(g_adc_state.mutex);

    ESP_LOGD(TAG, "Read: U=%.2fV (rms), I=%.3fA (rms)",
             measurement->voltage_rms, measurement->current_rms);

    return ESP_OK;
}

esp_err_t adc_sensor_get_calibration(adc_calibration_t *calib)
{
    if (calib == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_adc_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!xSemaphoreTake(g_adc_state.mutex, pdMS_TO_TICKS(SYNC_TIMEOUT_MS))) {
        return ESP_ERR_TIMEOUT;
    }

    memcpy(calib, &g_adc_state.calibration, sizeof(adc_calibration_t));

    xSemaphoreGive(g_adc_state.mutex);

    return ESP_OK;
}

esp_err_t adc_sensor_set_calibration(const adc_calibration_t *calib)
{
    if (calib == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Validate calibration values
    esp_err_t ret = adc_validate_calibration(calib);
    if (ret != ESP_OK) {
        return ret;
    }

    if (!g_adc_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!xSemaphoreTake(g_adc_state.mutex, pdMS_TO_TICKS(SYNC_TIMEOUT_MS))) {
        return ESP_ERR_TIMEOUT;
    }

    memcpy(&g_adc_state.calibration, calib, sizeof(adc_calibration_t));

    xSemaphoreGive(g_adc_state.mutex);

    ESP_LOGI(TAG, "Calibration updated: V(offset=%.3f, scale=%.6f), "
             "I(offset=%.3f, scale=%.6f)",
             calib->voltage_offset, calib->voltage_scale,
             calib->current_offset, calib->current_scale);

    return ESP_OK;
}

esp_err_t adc_sensor_save_calibration(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;

    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: 0x%02x", ret);
        return ret;
    }

    if (!xSemaphoreTake(g_adc_state.mutex, pdMS_TO_TICKS(SYNC_TIMEOUT_MS))) {
        nvs_close(nvs_handle);
        return ESP_ERR_TIMEOUT;
    }

    // Save calibration values
    ret |= nvs_set_blob(nvs_handle, NVS_KEY_VOLTAGE_OFFSET,
                       &g_adc_state.calibration.voltage_offset,
                       sizeof(float));
    ret |= nvs_set_blob(nvs_handle, NVS_KEY_VOLTAGE_SCALE,
                       &g_adc_state.calibration.voltage_scale,
                       sizeof(float));
    ret |= nvs_set_blob(nvs_handle, NVS_KEY_CURRENT_OFFSET,
                       &g_adc_state.calibration.current_offset,
                       sizeof(float));
    ret |= nvs_set_blob(nvs_handle, NVS_KEY_CURRENT_SCALE,
                       &g_adc_state.calibration.current_scale,
                       sizeof(float));

    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }

    xSemaphoreGive(g_adc_state.mutex);
    nvs_close(nvs_handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS save failed: 0x%02x", ret);
        return ret;
    }

    ESP_LOGI(TAG, "Calibration saved to NVS");
    return ESP_OK;
}

esp_err_t adc_sensor_load_calibration(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;

    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed, using defaults: 0x%02x", ret);
        adc_sensor_reset_calibration();
        return ESP_OK;  // Not an error - use defaults
    }

    size_t size = sizeof(float);
    adc_calibration_t temp_calib = {0};

    // Load values with fallbacks
    if (nvs_get_blob(nvs_handle, NVS_KEY_VOLTAGE_OFFSET,
                    &temp_calib.voltage_offset, &size) != ESP_OK) {
        temp_calib.voltage_offset = 0.0f;
    }

    if (nvs_get_blob(nvs_handle, NVS_KEY_VOLTAGE_SCALE,
                    &temp_calib.voltage_scale, &size) != ESP_OK) {
        temp_calib.voltage_scale = 1.0f;
    }

    if (nvs_get_blob(nvs_handle, NVS_KEY_CURRENT_OFFSET,
                    &temp_calib.current_offset, &size) != ESP_OK) {
        temp_calib.current_offset = 0.0f;
    }

    if (nvs_get_blob(nvs_handle, NVS_KEY_CURRENT_SCALE,
                    &temp_calib.current_scale, &size) != ESP_OK) {
        temp_calib.current_scale = 1.0f;
    }

    nvs_close(nvs_handle);

    // Validate and apply
    ret = adc_validate_calibration(&temp_calib);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Invalid NVS calibration, using defaults");
        adc_sensor_reset_calibration();
        return ESP_OK;
    }

    if (!xSemaphoreTake(g_adc_state.mutex, pdMS_TO_TICKS(SYNC_TIMEOUT_MS))) {
        return ESP_ERR_TIMEOUT;
    }

    memcpy(&g_adc_state.calibration, &temp_calib, sizeof(adc_calibration_t));

    xSemaphoreGive(g_adc_state.mutex);

    ESP_LOGI(TAG, "Calibration loaded from NVS");
    return ESP_OK;
}

esp_err_t adc_sensor_reset_calibration(void)
{
    adc_calibration_t default_calib = {
        .voltage_offset = 0.0f,
        .voltage_scale = 1.0f,
        .current_offset = 0.0f,
        .current_scale = 1.0f
    };

    return adc_sensor_set_calibration(&default_calib);
}

esp_err_t adc_sensor_get_status(uint32_t *samples_processed,
                               uint32_t *buffer_overflows,
                               uint32_t *read_timeouts)
{
    if (samples_processed == NULL || buffer_overflows == NULL ||
        read_timeouts == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_adc_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!xSemaphoreTake(g_adc_state.mutex, pdMS_TO_TICKS(SYNC_TIMEOUT_MS))) {
        return ESP_ERR_TIMEOUT;
    }

    *samples_processed = g_adc_state.samples_processed;
    *buffer_overflows = g_adc_state.buffer_overflows;
    *read_timeouts = g_adc_state.read_timeouts;

    xSemaphoreGive(g_adc_state.mutex);

    return ESP_OK;
}

// ============================================================================
// Internal Function Implementation
// ============================================================================

static bool adc_isr_callback(adc_continuous_handle_t handle,
                           const adc_continuous_evt_data_t *edata,
                           void *user_data)
{
    // CRITICAL: This runs in ISR context at 16000 calls/second!
    // Must complete in <100 microseconds or samples drop.

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Process ADC data (2 bytes per channel: voltage + current)
    uint8_t *data = (uint8_t *)edata->conv_frame_buffer;
    uint32_t num_samples = edata->size / 4;  // 2 bytes * 2 channels

    for (uint32_t i = 0; i < num_samples; i++) {
        // Extract voltage and current samples (12-bit, MSB first)
        uint16_t voltage = (data[i*4 + 1] << 8) | data[i*4 + 0];
        uint16_t current = (data[i*4 + 3] << 8) | data[i*4 + 2];

        // Store in ring buffer
        g_adc_state.buffer.voltage[g_adc_state.buffer.head] = voltage;
        g_adc_state.buffer.current[g_adc_state.buffer.head] = current;

        g_adc_state.buffer.head = (g_adc_state.buffer.head + 1) %
                                 ADC_RMS_WINDOW_SAMPLES;

        if (g_adc_state.buffer.count < ADC_RMS_WINDOW_SAMPLES) {
            g_adc_state.buffer.count++;
        } else {
            g_adc_state.buffer_overflows++;
        }

        g_adc_state.samples_processed++;
    }

    return (xHigherPriorityTaskWoken == pdTRUE);
}

static void adc_processing_task(void *arg)
{
    ESP_LOGI(TAG, "ADC processing task started");

    uint8_t raw_buffer[2048];

    while (g_adc_state.running) {
        size_t bytes_read = 0;
        esp_err_t err = adc_driver_read(g_adc_state.handle,
                                       raw_buffer,
                                       sizeof(raw_buffer),
                                       &bytes_read,
                                       pdMS_TO_TICKS(ADC_READ_TIMEOUT_MS));

        if (err == ESP_OK && bytes_read > 0) {
            adc_process_raw_data(raw_buffer, bytes_read);
        } else if (err == ESP_ERR_TIMEOUT) {
            ESP_LOGD(TAG, "ADC read timeout");
        } else if (err != ESP_OK) {
            ESP_LOGW(TAG, "ADC read failed: 0x%02x", err);
        }

        vTaskDelay(pdMS_TO_TICKS(TASK_IDLE_DELAY_MS));
    }

    ESP_LOGI(TAG, "ADC processing task ended");
    vTaskDelete(NULL);
}

static void adc_process_raw_data(const uint8_t *data, size_t size)
{
    if (data == NULL || size < sizeof(adc_digi_output_data_t)) {
        return;
    }

    const adc_digi_output_data_t *samples = (const adc_digi_output_data_t *)data;
    size_t sample_count = size / sizeof(adc_digi_output_data_t);
    uint32_t pending_voltage = 0;
    uint32_t pending_current = 0;
    bool have_voltage = false;
    bool have_current = false;

    for (size_t i = 0; i < sample_count; ++i) {
        const adc_digi_output_data_t sample = samples[i];
        uint32_t raw_value = sample.type1.data;
        adc_channel_t channel = sample.type1.channel;

        if (channel == ADC_CHANNEL_VOLTAGE) {
            pending_voltage = raw_value;
            have_voltage = true;
        } else if (channel == ADC_CHANNEL_CURRENT) {
            pending_current = raw_value;
            have_current = true;
        }

        if (have_voltage && have_current) {
            g_adc_state.buffer.voltage[g_adc_state.buffer.head] = (uint16_t)pending_voltage;
            g_adc_state.buffer.current[g_adc_state.buffer.head] = (uint16_t)pending_current;

            g_adc_state.buffer.head = (g_adc_state.buffer.head + 1) % ADC_RMS_WINDOW_SAMPLES;
            if (g_adc_state.buffer.count < ADC_RMS_WINDOW_SAMPLES) {
                g_adc_state.buffer.count++;
            } else {
                g_adc_state.buffer_overflows++;
            }

            have_voltage = false;
            have_current = false;
        }
    }
}

static esp_err_t adc_configure_hardware(void)
{
    if (g_adc_state.handle != NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Configuring ADC hardware via driver...");
    esp_err_t err = adc_driver_init(&g_adc_state.handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_driver_init failed: 0x%02x", err);
    }
    return err;
}

static esp_err_t adc_start_sampling(void)
{
    if (g_adc_state.running) {
        return ESP_OK;
    }

    esp_err_t err = adc_driver_start(g_adc_state.handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_driver_start failed: 0x%02x", err);
        return err;
    }

    g_adc_state.running = true;
    ESP_LOGI(TAG, "ADC sampling started");
    return ESP_OK;
}

static esp_err_t adc_stop_sampling(void)
{
    if (!g_adc_state.running) {
        return ESP_OK;
    }

    esp_err_t err = adc_driver_stop(g_adc_state.handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "adc_driver_stop failed: 0x%02x", err);
    }

    g_adc_state.running = false;
    return err;
}

static float adc_calculate_rms(const uint16_t *samples, uint32_t count)
{
    if (samples == NULL || count == 0) {
        return 0.0f;
    }

    double sum_squares = 0.0;

    for (uint32_t i = 0; i < count; i++) {
        double sample = (double)samples[i];
        sum_squares += sample * sample;
    }

    double mean_square = sum_squares / count;
    double rms = sqrt(mean_square);

    return (float)rms;
}

static esp_err_t adc_validate_calibration(const adc_calibration_t *calib)
{
    if (calib == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check for reasonable scale values (0.1 to 10.0)
    if (calib->voltage_scale < 0.1f || calib->voltage_scale > 10.0f) {
        ESP_LOGE(TAG, "Invalid voltage scale: %.6f", calib->voltage_scale);
        return ESP_ERR_INVALID_ARG;
    }

    if (calib->current_scale < 0.1f || calib->current_scale > 10.0f) {
        ESP_LOGE(TAG, "Invalid current scale: %.6f", calib->current_scale);
        return ESP_ERR_INVALID_ARG;
    }

    // Check for reasonable offset values (±10% of full scale)
    const float max_offset = 409.5f;  // 10% of 4095

    if (fabsf(calib->voltage_offset) > max_offset) {
        ESP_LOGE(TAG, "Invalid voltage offset: %.3f", calib->voltage_offset);
        return ESP_ERR_INVALID_ARG;
    }

    if (fabsf(calib->current_offset) > max_offset) {
        ESP_LOGE(TAG, "Invalid current offset: %.3f", calib->current_offset);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

#endif
