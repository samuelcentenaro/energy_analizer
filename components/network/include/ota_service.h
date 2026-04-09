/**
 * @file ota_service.h
 * @brief Over-The-Air (OTA) Firmware Update Service
 * 
 * Provides remote firmware update capability via WiFi with:
 * - Automatic version checking
 * - Secure download with hash validation
 * - Automatic rollback on corruption
 * - HTTPS with certificate validation
 * 
 * @note Depends on WiFi being configured (Phase 4)
 * @note Uses ESP-IDF esp_https_ota component
 */

#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OTA update state enumeration
 */
typedef enum {
    OTA_STATE_IDLE,           ///< No update in progress
    OTA_STATE_CHECKING,       ///< Checking server for new version
    OTA_STATE_DOWNLOADING,    ///< Download in progress
    OTA_STATE_VALIDATING,     ///< Validating firmware integrity
    OTA_STATE_SUCCESS,        ///< Update completed successfully
    OTA_STATE_FAILED,         ///< Update failed with error
    OTA_STATE_ABORTED,        ///< User cancelled update
} ota_state_t;

/**
 * @brief OTA error codes
 */
typedef enum {
    OTA_ERR_NONE = 0,
    OTA_ERR_NETWORK,          ///< Network/WiFi error
    OTA_ERR_SERVER,           ///< Server error (4xx/5xx)
    OTA_ERR_INVALID_URL,      ///< Invalid firmware URL format
    OTA_ERR_HASH_MISMATCH,    ///< Downloaded firmware hash invalid
    OTA_ERR_SIGNATURE_INVALID, ///< Firmware signature verification failed
    OTA_ERR_TIMEOUT,          ///< Download timeout
    OTA_ERR_STORAGE,          ///< Flash storage error
    OTA_ERR_MEMORY,           ///< Insufficient memory
    OTA_ERR_INTERNAL,         ///< Internal service error
} ota_error_t;

/**
 * @brief OTA progress information
 */
typedef struct {
    ota_state_t state;              ///< Current OTA state
    ota_error_t error;              ///< Last error code (if state == FAILED)
    uint32_t bytes_downloaded;      ///< Bytes downloaded so far
    uint32_t total_bytes;           ///< Total bytes to download (0 if unknown)
    uint8_t  progress_percent;      ///< Progress 0-100% (0 if unknown)
    char     current_version[32];   ///< Currently running version
    char     available_version[32]; ///< Available version from server
} ota_progress_t;

/**
 * @brief Callback function for OTA progress updates
 * 
 * @param[in] progress Current progress information
 * @param[in] user_data User-provided context
 */
typedef void (*ota_progress_callback_t)(const ota_progress_t *progress, 
                                        void *user_data);

/**
 * @brief Initialize OTA service
 * 
 * Starts the OTA background task and initializes state.
 * Must be called once during application startup.
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_NO_MEM if insufficient memory for task
 * @return ESP_ERR_INVALID_STATE if already initialized
 */
esp_err_t ota_service_init(void);

/**
 * @brief Deinitialize OTA service
 * 
 * Stops the OTA task. Safe to call even if not initialized.
 * 
 * @return ESP_OK always
 */
esp_err_t ota_service_deinit(void);

/**
 * @brief Check for firmware updates on server
 * 
 * Connects to version server and checks if newer firmware is available.
 * Non-blocking - result provided via callback.
 * 
 * @return ESP_OK if check started
 * @return ESP_ERR_INVALID_STATE if service not initialized
 * @return ESP_ERR_NO_MEM if insufficient memory
 */
esp_err_t ota_check_for_updates(void);

/**
 * @brief Perform firmware update
 * 
 * Downloads firmware from specified URL, validates, and installs.
 * Blocks until complete or error. On success, triggers reboot.
 * 
 * @param[in] firmware_url URL to firmware binary (http or https)
 * @return ESP_OK if update completed (will reboot before returning)
 * @return ESP_ERR_INVALID_ARG if URL is NULL or invalid
 * @return ESP_ERR_INVALID_STATE if service not initialized
 * @return OTA_ERR_* on update failure
 */
esp_err_t ota_perform_update(const char *firmware_url);

/**
 * @brief Get current OTA service state
 * 
 * @return Current OTA state (see ota_state_t)
 */
ota_state_t ota_get_state(void);

/**
 * @brief Get detailed OTA progress information
 * 
 * @param[out] progress Pointer to progress structure to fill
 * @return ESP_OK if progress retrieved
 * @return ESP_ERR_INVALID_ARG if progress is NULL
 * @return ESP_ERR_INVALID_STATE if service not initialized
 */
esp_err_t ota_get_progress(ota_progress_t *progress);

/**
 * @brief Cancel ongoing OTA operation
 * 
 * Safe to call even if no operation in progress.
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if service not initialized
 */
esp_err_t ota_cancel_update(void);

/**
 * @brief Register callback for progress updates
 * 
 * Callback called whenever OTA state changes.
 * 
 * @param[in] callback Callback function (NULL to disable)
 * @param[in] user_data User context passed to callback
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if service not initialized
 */
esp_err_t ota_register_progress_callback(ota_progress_callback_t callback,
                                         void *user_data);

/**
 * @brief Get current application version string
 * 
 * @param[out] version Buffer for version string (minimum 32 bytes)
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if version is NULL
 */
esp_err_t ota_get_current_version(char *version);

/**
 * @brief Set OTA server URL
 * 
 * Configures the server URL for version checking and firmware download.
 * Persisted in NVS.
 * 
 * @param[in] url Server base URL (e.g., "https://firmware.example.com")
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if url is NULL
 * @return ESP_ERR_INVALID_SIZE if url too long
 */
esp_err_t ota_set_server_url(const char *url);

/**
 * @brief Get OTA server URL
 * 
 * Retrieves currently configured server URL.
 * 
 * @param[out] url Buffer for URL (minimum 256 bytes)
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if url is NULL
 */
esp_err_t ota_get_server_url(char *url);

/**
 * @brief Get OTA service statistics
 * 
 * @param[out] total_updates Total number of successful updates
 * @param[out] failed_attempts Number of failed update attempts
 * @param[out] last_update_time Timestamp of last successful update (seconds since epoch)
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if any pointer is NULL
 */
esp_err_t ota_get_statistics(uint32_t *total_updates, 
                             uint32_t *failed_attempts,
                             uint32_t *last_update_time);

#ifdef __cplusplus
}
#endif

#endif // OTA_SERVICE_H
