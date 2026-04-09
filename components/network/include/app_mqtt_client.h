/**
 * @file mqtt_client.h
 * @brief MQTT telemetry client for Energy Analyzer
 */

#ifndef APP_MQTT_CLIENT_H
#define APP_MQTT_CLIENT_H

#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"
#include "common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// MQTT Status & State
// ============================================================================

/// @brief MQTT client status information
typedef struct {
    mqtt_state_t state;               ///< Current connection state
    char broker_url[128];             ///< Connected broker URL
    uint32_t connected_at_ms;         ///< Timestamp when connected
    uint32_t messages_published;      ///< Count of published messages
    uint32_t messages_failed;         ///< Count of failed publishes
    uint32_t last_published_ms;       ///< Timestamp of last publish
    bool auto_reconnect_enabled;      ///< Auto-reconnect status
} mqtt_client_status_t;

/// @brief MQTT telemetry payload
typedef struct {
    float voltage_rms;        ///< RMS voltage in Volts
    float current_rms;        ///< RMS current in Amperes
    float power_real;         ///< Real power in Watts
    float power_factor;       ///< Power factor (0-1)
    uint32_t timestamp_ms;    ///< Measurement timestamp
} mqtt_telemetry_t;

// ============================================================================
// MQTT Initialization & Control
// ============================================================================

/**
 * @brief Initialize MQTT client component
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mqtt_client_init(void);

/**
 * @brief Start MQTT client (connect to broker)
 * @param config MQTT configuration (broker URL, credentials, etc.)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mqtt_client_start(const app_mqtt_config_t *config);

/**
 * @brief Get the active MQTT configuration
 * @param[out] config Pointer to receive the current configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mqtt_client_get_config(app_mqtt_config_t *config);

/**
 * @brief Apply a new MQTT configuration and persist it to NVS
 * @param[in] config Pointer to the new configuration
 * @param[in] restart_if_running Restart the MQTT runtime when currently active
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mqtt_client_apply_config(const app_mqtt_config_t *config, bool restart_if_running);

/**
 * @brief Publish measurement telemetry
 * @param telemetry Measurement data to publish
 * @return ESP_OK on success, error code otherwise
 * @note Non-blocking; queues message internally
 */
esp_err_t mqtt_client_publish_telemetry(const mqtt_telemetry_t *telemetry);

/**
 * @brief Get current MQTT client status
 * @return Status structure
 */
mqtt_client_status_t mqtt_client_get_status(void);

/**
 * @brief Check if MQTT is connected
 * @return true if connected, false otherwise
 */
bool mqtt_client_is_connected(void);

/**
 * @brief Stop MQTT client (disconnect gracefully)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mqtt_client_stop(void);

/**
 * @brief Deinitialize MQTT client component
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mqtt_client_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // APP_MQTT_CLIENT_H
