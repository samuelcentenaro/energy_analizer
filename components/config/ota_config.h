/**
 * @file ota_config.h
 * @brief OTA Service Configuration Constants
 * 
 * Compile-time configuration for Over-The-Air firmware updates.
 */

#ifndef OTA_CONFIG_H
#define OTA_CONFIG_H

// ============================================================================
// OTA Server Configuration
// ============================================================================

/// @brief Default firmware server base URL
/// @note Can be overridden at runtime with ota_set_server_url()
#define OTA_SERVER_URL_DEFAULT      "https://firmware.example.com"

/// @brief API endpoint for version checking (appended to server URL)
#define OTA_VERSION_CHECK_ENDPOINT  "/api/version"

/// @brief Firmware download endpoint format
/// @note Must contain a %s placeholder for firmware filename
#define OTA_FIRMWARE_ENDPOINT       "/firmware/%s.bin"

// ============================================================================
// OTA Behavior Configuration
// ============================================================================

/// @brief Enable automatic update checking (every 24 hours)
#define OTA_ENABLE_AUTO_CHECK       1

/// @brief Automatic check interval in seconds (24 hours)
#define OTA_CHECK_INTERVAL_S        (24 * 60 * 60)

/// @brief Allow manual update trigger via menu
#define OTA_ENABLE_MANUAL_UPDATE    1

/// @brief Display update progress on OLED
#define OTA_DISPLAY_PROGRESS        1

// ============================================================================
// OTA Security Configuration
// ============================================================================

/// @brief Enable HTTPS (TLS) for secure download
#define OTA_ENABLE_HTTPS            1

/// @brief Enable firmware signature verification (RSA-2048)
#define OTA_ENABLE_SIGNATURE_CHECK  1

/// @brief Enable hash validation (SHA-256) of downloaded firmware
#define OTA_ENABLE_HASH_VALIDATION  1

/// @brief Enable certificate pinning (public key pinning)
#define OTA_ENABLE_CERT_PINNING     1

/// @brief Maximum download timeout in seconds
#define OTA_DOWNLOAD_TIMEOUT_S      300  // 5 minutes

// ============================================================================
// OTA Storage Configuration
// ============================================================================

/// @brief NVS namespace for OTA settings and logs
#define OTA_NVS_NAMESPACE           "ota_config"

/// @brief NVS key for server URL
#define OTA_NVS_KEY_SERVER_URL      "server_url"

/// @brief NVS key for last successful update timestamp
#define OTA_NVS_KEY_LAST_UPDATE     "last_update"

/// @brief NVS key for total successful updates count
#define OTA_NVS_KEY_UPDATE_COUNT    "update_count"

/// @brief NVS key for failed update attempts count
#define OTA_NVS_KEY_FAILED_COUNT    "failed_count"

/// @brief Maximum length of server URL in NVS
#define OTA_MAX_URL_LENGTH          256

// ============================================================================
// OTA Task Configuration
// ============================================================================

/// @brief OTA service task stack size (bytes)
#define OTA_TASK_STACK_SIZE         4096

/// @brief OTA service task priority (0=idle, configMAX_PRIORITIES-1=highest)
#define OTA_TASK_PRIORITY           2

/// @brief Task name for debugging
#define OTA_TASK_NAME               "ota_service"

// ============================================================================
// OTA Version Configuration
// ============================================================================

/// @brief Application firmware version string
/// @note Format: "major.minor.patch" (e.g., "1.0.0")
#define APP_VERSION_MAJOR           1
#define APP_VERSION_MINOR           0
#define APP_VERSION_PATCH           0

/// @brief Create version string macro
#define APP_VERSION_STRING          \
    _STR(APP_VERSION_MAJOR) "." \
    _STR(APP_VERSION_MINOR) "." \
    _STR(APP_VERSION_PATCH)

#define _STR(x)  #x

// ============================================================================
// OTA Debug Configuration
// ============================================================================

/// @brief Enable verbose OTA logging
#define OTA_ENABLE_LOGGING          1

/// @brief Log update history to NVS (requires space)
#define OTA_LOG_UPDATE_HISTORY      1

/// @brief Maximum history entries
#define OTA_MAX_HISTORY_ENTRIES     10

#endif // OTA_CONFIG_H
