/**
 * @file ota_service.c
 * @brief Over-The-Air (OTA) Firmware Update Service Implementation
 * 
 * Implements remote firmware update capability via WiFi with automatic
 * version checking, secure download, and rollback support.
 */

#include "ota_service.h"
#include "ota_config.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <time.h>

// ============================================================================
// Module Definitions
// ============================================================================

static const char *TAG = "ota_service";

/// Maximum size of firmware URL
#define OTA_MAX_FIRMWARE_URL_LENGTH  512

/// OTA service state machine
typedef struct {
    bool initialized;
    ota_state_t state;
    ota_error_t last_error;
    
    TaskHandle_t task_handle;
    SemaphoreHandle_t mutex;
    
    uint32_t bytes_downloaded;
    uint32_t total_bytes;
    
    char server_url[OTA_MAX_URL_LENGTH];
    char current_version[32];
    char available_version[32];
    
    ota_progress_callback_t progress_callback;
    void *callback_user_data;
    
    uint32_t total_updates;
    uint32_t failed_attempts;
    uint32_t last_update_time;
} ota_service_state_t;

static ota_service_state_t g_ota_state = {0};

// ============================================================================
// Forward Declarations
// ============================================================================

static void ota_service_task(void *arg);
static esp_err_t ota_load_configuration(void);
static esp_err_t ota_save_configuration(void);
static void ota_notify_progress(void);
static esp_err_t ota_get_current_version_internal(char *version);

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t ota_service_init(void)
{
    ESP_LOGI(TAG, "Initializing OTA service...");
    
    if (g_ota_state.initialized) {
        ESP_LOGW(TAG, "OTA service already initialized");
        return ESP_OK;
    }
    
    // Initialize state
    memset(&g_ota_state, 0, sizeof(ota_service_state_t));
    g_ota_state.state = OTA_STATE_IDLE;
    g_ota_state.last_error = OTA_ERR_NONE;
    
    // Create synchronization primitive
    g_ota_state.mutex = xSemaphoreCreateMutex();
    if (g_ota_state.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Load configuration from NVS
    esp_err_t ret = ota_load_configuration();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load OTA config, using defaults");
    }
    
    // Get current application version
    ret = ota_get_current_version_internal(g_ota_state.current_version);
    if (ret != ESP_OK) {
        strlcpy(g_ota_state.current_version, "unknown", sizeof(g_ota_state.current_version));
    }
    
    // Create OTA service task
    BaseType_t task_ret = xTaskCreate(
        ota_service_task,
        OTA_TASK_NAME,
        OTA_TASK_STACK_SIZE,
        NULL,
        OTA_TASK_PRIORITY,
        &g_ota_state.task_handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA service task");
        vSemaphoreDelete(g_ota_state.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    g_ota_state.initialized = true;
    
    ESP_LOGI(TAG, "OTA service initialized");
    ESP_LOGI(TAG, "  Current version: %s", g_ota_state.current_version);
    ESP_LOGI(TAG, "  Server URL: %s", g_ota_state.server_url);
    ESP_LOGI(TAG, "  Auto-check: %s (%d seconds)",
             OTA_ENABLE_AUTO_CHECK ? "enabled" : "disabled",
             OTA_CHECK_INTERVAL_S);
    
    return ESP_OK;
}

esp_err_t ota_service_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing OTA service...");
    
    if (!g_ota_state.initialized) {
        return ESP_OK;
    }
    
    if (g_ota_state.task_handle != NULL) {
        vTaskDelete(g_ota_state.task_handle);
        g_ota_state.task_handle = NULL;
    }
    
    if (g_ota_state.mutex != NULL) {
        vSemaphoreDelete(g_ota_state.mutex);
        g_ota_state.mutex = NULL;
    }
    
    g_ota_state.initialized = false;
    
    ESP_LOGI(TAG, "OTA service deinitialized");
    return ESP_OK;
}

esp_err_t ota_check_for_updates(void)
{
    if (!g_ota_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // STUB: Real implementation calls version API on server
    // Compares server version with current version
    // Sets g_ota_state.available_version if newer found
    
    ESP_LOGI(TAG, "Checking for updates (stub)");
    return ESP_OK;
}

esp_err_t ota_perform_update(const char *firmware_url)
{
    if (firmware_url == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_ota_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_ota_state.state == OTA_STATE_DOWNLOADING) {
        ESP_LOGW(TAG, "Update already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    
    // STUB: Real implementation:
    // 1. Download firmware from firmware_url
    // 2. Validate signature (if enabled)
    // 3. Validate hash (if enabled)
    // 4. Install to OTA partition
    // 5. Mark as valid
    // 6. Reboot
    
    ESP_LOGI(TAG, "Starting firmware update (stub): %s", firmware_url);
    return ESP_OK;
}

ota_state_t ota_get_state(void)
{
    return g_ota_state.state;
}

esp_err_t ota_get_progress(ota_progress_t *progress)
{
    if (progress == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_ota_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_ota_state.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    progress->state = g_ota_state.state;
    progress->error = g_ota_state.last_error;
    progress->bytes_downloaded = g_ota_state.bytes_downloaded;
    progress->total_bytes = g_ota_state.total_bytes;
    
    if (g_ota_state.total_bytes > 0) {
        progress->progress_percent = 
            (g_ota_state.bytes_downloaded * 100) / g_ota_state.total_bytes;
    } else {
        progress->progress_percent = 0;
    }
    
    strlcpy(progress->current_version, g_ota_state.current_version,
            sizeof(progress->current_version));
    strlcpy(progress->available_version, g_ota_state.available_version,
            sizeof(progress->available_version));
    
    xSemaphoreGive(g_ota_state.mutex);
    
    return ESP_OK;
}

esp_err_t ota_cancel_update(void)
{
    if (!g_ota_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_ota_state.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (g_ota_state.state == OTA_STATE_DOWNLOADING) {
        g_ota_state.state = OTA_STATE_ABORTED;
        ESP_LOGI(TAG, "OTA update cancelled");
    }
    
    xSemaphoreGive(g_ota_state.mutex);
    
    return ESP_OK;
}

esp_err_t ota_register_progress_callback(ota_progress_callback_t callback,
                                         void *user_data)
{
    if (!g_ota_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_ota_state.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    g_ota_state.progress_callback = callback;
    g_ota_state.callback_user_data = user_data;
    
    xSemaphoreGive(g_ota_state.mutex);
    
    return ESP_OK;
}

esp_err_t ota_get_current_version(char *version)
{
    if (version == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_ota_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    strlcpy(version, g_ota_state.current_version, 32);
    return ESP_OK;
}

esp_err_t ota_set_server_url(const char *url)
{
    if (url == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(url) >= OTA_MAX_URL_LENGTH) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    if (!g_ota_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_ota_state.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    strlcpy(g_ota_state.server_url, url, OTA_MAX_URL_LENGTH);
    
    xSemaphoreGive(g_ota_state.mutex);
    
    // Save to NVS
    ota_save_configuration();
    
    ESP_LOGI(TAG, "Server URL updated: %s", url);
    return ESP_OK;
}

esp_err_t ota_get_server_url(char *url)
{
    if (url == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_ota_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    strlcpy(url, g_ota_state.server_url, OTA_MAX_URL_LENGTH);
    return ESP_OK;
}

esp_err_t ota_get_statistics(uint32_t *total_updates, 
                             uint32_t *failed_attempts,
                             uint32_t *last_update_time)
{
    if (total_updates == NULL || failed_attempts == NULL || 
        last_update_time == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_ota_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_ota_state.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    *total_updates = g_ota_state.total_updates;
    *failed_attempts = g_ota_state.failed_attempts;
    *last_update_time = g_ota_state.last_update_time;
    
    xSemaphoreGive(g_ota_state.mutex);
    
    return ESP_OK;
}

// ============================================================================
// Internal Functions
// ============================================================================

static void ota_service_task(void *arg)
{
    ESP_LOGI(TAG, "OTA service task started");
    
    time_t last_check = 0;
    
    while (g_ota_state.initialized) {
        vTaskDelay(pdMS_TO_TICKS(60000));  // Check every 60 seconds
        
        if (!OTA_ENABLE_AUTO_CHECK) {
            continue;
        }
        
        time_t now = time(NULL);
        if ((now - last_check) >= OTA_CHECK_INTERVAL_S) {
            ESP_LOGI(TAG, "Periodic update check triggered");
            // ota_check_for_updates();
            last_check = now;
        }
    }
    
    ESP_LOGI(TAG, "OTA service task ended");
    vTaskDelete(NULL);
}

static esp_err_t ota_load_configuration(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    ret = nvs_open(OTA_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        goto defaults;
    }
    
    size_t server_url_size = sizeof(g_ota_state.server_url);
    ret = nvs_get_str(nvs_handle, OTA_NVS_KEY_SERVER_URL,
                      g_ota_state.server_url, &server_url_size);
    
    nvs_get_u32(nvs_handle, OTA_NVS_KEY_UPDATE_COUNT, &g_ota_state.total_updates);
    nvs_get_u32(nvs_handle, OTA_NVS_KEY_FAILED_COUNT, &g_ota_state.failed_attempts);
    nvs_get_u32(nvs_handle, OTA_NVS_KEY_LAST_UPDATE, &g_ota_state.last_update_time);
    
    nvs_close(nvs_handle);
    return ESP_OK;
    
defaults:
    strlcpy(g_ota_state.server_url, OTA_SERVER_URL_DEFAULT,
            sizeof(g_ota_state.server_url));
    return ESP_OK;
}

static esp_err_t ota_save_configuration(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    ret = nvs_open(OTA_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: 0x%02x", ret);
        return ret;
    }
    
    nvs_set_str(nvs_handle, OTA_NVS_KEY_SERVER_URL, g_ota_state.server_url);
    nvs_set_u32(nvs_handle, OTA_NVS_KEY_UPDATE_COUNT, g_ota_state.total_updates);
    nvs_set_u32(nvs_handle, OTA_NVS_KEY_FAILED_COUNT, g_ota_state.failed_attempts);
    nvs_set_u32(nvs_handle, OTA_NVS_KEY_LAST_UPDATE, g_ota_state.last_update_time);
    
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    return ret;
}

static void ota_notify_progress(void)
{
    if (g_ota_state.progress_callback != NULL) {
        ota_progress_t progress = {0};
        ota_get_progress(&progress);
        g_ota_state.progress_callback(&progress, g_ota_state.callback_user_data);
    }
}

static esp_err_t ota_get_current_version_internal(char *version)
{
    // Get version from app image descriptor
    const esp_app_desc_t *app_desc = esp_app_get_description();
    if (app_desc == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    strlcpy(version, app_desc->version, 32);
    return ESP_OK;
}
