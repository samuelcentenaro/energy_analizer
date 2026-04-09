/**
 * @file mqtt_client.c
 * @brief MQTT telemetry client implementation
 * @details Manages MQTT broker connection and publishes measurement telemetry
 *          using the ESP-MQTT library (esp_mqtt_client).
 */

#include "app_mqtt_client.h"

#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "MQTT_CLIENT";

// ============================================================================
// Configuration & Constants
// ============================================================================

#define MQTT_NVS_NAMESPACE       "mqtt_cfg"
#define MQTT_NVS_KEY_BROKER      "broker_url"
#define MQTT_NVS_KEY_PORT        "port"
#define MQTT_NVS_KEY_CLIENT_ID   "client_id"
#define MQTT_TASK_PRIORITY       4
#define MQTT_TASK_STACK_SIZE     4096
#define MQTT_QUEUE_SIZE          10
#define MQTT_TOPIC_BUFFER_SIZE   128
#define MQTT_JSON_BUFFER_SIZE    256
#define MQTT_RECONNECT_DELAY_MS  5000
#define MQTT_PUBLISH_TIMEOUT_MS  5000
#define MQTT_DISCONNECTED_PAUSE_MS 1000

// ============================================================================
// Internal State
// ============================================================================

static bool g_initialized = false;
static bool g_started = false;
static mqtt_client_status_t g_status = {
    .state = MQTT_STATE_DISCONNECTED,
    .broker_url = {0},
    .connected_at_ms = 0,
    .messages_published = 0,
    .messages_failed = 0,
    .last_published_ms = 0,
    .auto_reconnect_enabled = true
};

static app_mqtt_config_t g_config = {
    .broker_url = CONFIG_APP_MQTT_BROKER_HOST,
    .port = CONFIG_APP_MQTT_BROKER_PORT,
    .client_id = CONFIG_APP_MQTT_CLIENT_ID,
    .username = {0},
    .password = {0},
    .keep_alive = CONFIG_APP_MQTT_KEEP_ALIVE
};

static esp_mqtt_client_handle_t g_mqtt_handle = NULL;
static TaskHandle_t g_dispatcher_task = NULL;
static QueueHandle_t g_publish_queue = NULL;
static bool g_should_stop = false;
static volatile bool g_mqtt_connected = false;

// ============================================================================
// Forward Declarations
// ============================================================================

static esp_err_t mqtt_client_open_nvs(nvs_handle_t *handle, nvs_open_mode_t mode);
static esp_err_t mqtt_client_load_config(void);
static esp_err_t mqtt_client_save_config(void);
static void mqtt_dispatcher_task(void *arg);
static void mqtt_client_update_state(mqtt_state_t state);
static esp_err_t mqtt_client_wait_dispatcher_stop(TickType_t timeout_ticks);
static const char *mqtt_client_state_text(mqtt_state_t state);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data);

static const char *mqtt_client_state_text(mqtt_state_t state)
{
    switch (state) {
        case MQTT_STATE_CONNECTING:
            return "CONNECTING";
        case MQTT_STATE_CONNECTED:
            return "CONNECTED";
        case MQTT_STATE_ERROR:
            return "ERROR";
        default:
            return "DISCONNECTED";
    }
}

// ============================================================================
// NVS Management
// ============================================================================

static esp_err_t mqtt_client_open_nvs(nvs_handle_t *handle, nvs_open_mode_t mode)
{
    esp_err_t ret;

    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = nvs_flash_init();
    if ((ret != ESP_OK) &&
        (ret != ESP_ERR_NVS_NO_FREE_PAGES) &&
        (ret != ESP_ERR_NVS_NEW_VERSION_FOUND)) {
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

    return nvs_open(MQTT_NVS_NAMESPACE, mode, handle);
}

static esp_err_t mqtt_client_load_config(void)
{
    nvs_handle_t handle = 0;
    size_t broker_len = sizeof(g_config.broker_url);
    size_t client_id_len = sizeof(g_config.client_id);
    esp_err_t ret;

    ret = mqtt_client_open_nvs(&handle, NVS_READONLY);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "Using default config (NVS not yet initialized)");
        return ESP_OK;
    }

    // Try to load broker URL
    ret = nvs_get_str(handle, MQTT_NVS_KEY_BROKER, g_config.broker_url, &broker_len);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return ret;
    }

    // Try to load port
    ret = nvs_get_u16(handle, MQTT_NVS_KEY_PORT, &g_config.port);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return ret;
    }

    // Try to load client ID
    ret = nvs_get_str(handle, MQTT_NVS_KEY_CLIENT_ID, g_config.client_id, &client_id_len);
    nvs_close(handle);

    if (ret == ESP_OK || ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Config loaded: broker=%s, port=%d, client_id=%s",
                 g_config.broker_url, g_config.port, g_config.client_id);
        return ESP_OK;
    }

    return ret;
}

static esp_err_t mqtt_client_save_config(void)
{
    nvs_handle_t handle = 0;
    esp_err_t ret;

    ret = mqtt_client_open_nvs(&handle, NVS_READWRITE);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_set_str(handle, MQTT_NVS_KEY_BROKER, g_config.broker_url);
    if (ret == ESP_OK) {
        ret = nvs_set_u16(handle, MQTT_NVS_KEY_PORT, g_config.port);
    }
    if (ret == ESP_OK) {
        ret = nvs_set_str(handle, MQTT_NVS_KEY_CLIENT_ID, g_config.client_id);
    }
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }

    nvs_close(handle);
    return ret;
}

// ============================================================================
// MQTT Event Handler
// ============================================================================

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    (void)handler_args;
    (void)base;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to broker");
            g_mqtt_connected = true;
            mqtt_client_update_state(MQTT_STATE_CONNECTED);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected from broker");
            g_mqtt_connected = false;
            mqtt_client_update_state(MQTT_STATE_DISCONNECTED);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGD(TAG, "Subscribed, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGD(TAG, "Unsubscribed, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "Published, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGD(TAG, "Received data on topic=%.*s", event->topic_len, event->topic);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT event error");
            if ((event != NULL) &&
                (event->error_handle != NULL) &&
                (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)) {
                ESP_LOGE(TAG, "  Last errno: %d", event->error_handle->esp_transport_sock_errno);
            }
            g_mqtt_connected = false;
            mqtt_client_update_state(MQTT_STATE_ERROR);
            break;

        default:
            ESP_LOGD(TAG, "Other MQTT event id=%d", (int)event_id);
            break;
    }
}

// ============================================================================
// State Management
// ============================================================================

static void mqtt_client_update_state(mqtt_state_t state)
{
    if (g_status.state != state) {
        ESP_LOGI(TAG, "MQTT state: %s -> %s",
                 mqtt_client_state_text(g_status.state),
                 mqtt_client_state_text(state));
        g_status.state = state;

        if (state == MQTT_STATE_CONNECTED) {
            g_status.connected_at_ms = (uint32_t)esp_log_timestamp();
        }
    }
}

static esp_err_t mqtt_client_wait_dispatcher_stop(TickType_t timeout_ticks)
{
    TickType_t start_tick = xTaskGetTickCount();

    while (g_dispatcher_task != NULL) {
        if ((xTaskGetTickCount() - start_tick) >= timeout_ticks) {
            return ESP_ERR_TIMEOUT;
        }

        vTaskDelay(pdMS_TO_TICKS(20U));
    }

    return ESP_OK;
}

// ============================================================================
// Dispatcher Task
// ============================================================================

static void mqtt_dispatcher_task(void *arg)
{
    mqtt_telemetry_t telemetry;
    static bool s_drop_logged = false;

    ESP_LOGI(TAG, "Dispatcher task started");

    while (!g_should_stop) {
        // Wait for queued telemetry messages
        if (xQueueReceive(g_publish_queue, &telemetry, pdMS_TO_TICKS(MQTT_DISCONNECTED_PAUSE_MS)) == pdTRUE) {
            if (!g_mqtt_connected) {
                if (!s_drop_logged) {
                    ESP_LOGW(TAG, "MQTT offline, dropping queued telemetry until reconnect");
                    s_drop_logged = true;
                }
                g_status.messages_failed++;
                continue;
            }

            s_drop_logged = false;

            char topic[MQTT_TOPIC_BUFFER_SIZE];
            char json_payload[MQTT_JSON_BUFFER_SIZE];

            // Build topic: energy-analyzer/device-id/telemetry
            snprintf(topic, sizeof(topic), "energy-analyzer/%s/telemetry",
                     g_config.client_id);

            // Build JSON payload
            int written = snprintf(json_payload, sizeof(json_payload),
                "{"
                "\"voltage_rms\":%.2f,"
                "\"current_rms\":%.3f,"
                "\"power_real\":%.1f,"
                "\"power_factor\":%.3f,"
                "\"timestamp\":%lu"
                "}",
                (double)telemetry.voltage_rms,
                (double)telemetry.current_rms,
                (double)telemetry.power_real,
                (double)telemetry.power_factor,
                (unsigned long)telemetry.timestamp_ms);

            if (written > 0 && written < (int)sizeof(json_payload)) {
                int msg_id = esp_mqtt_client_publish(g_mqtt_handle, topic, json_payload,
                                                      0, 0, 0);

                if (msg_id >= 0) {
                    g_status.messages_published++;
                    g_status.last_published_ms = (uint32_t)esp_log_timestamp();
                } else {
                    ESP_LOGE(TAG, "Publish failed (msg_id=%d)", msg_id);
                    g_status.messages_failed++;
                }
            } else {
                ESP_LOGE(TAG, "Failed to format JSON payload");
                g_status.messages_failed++;
            }
        }
    }

    ESP_LOGI(TAG, "Dispatcher task stopped");
    g_dispatcher_task = NULL;
    vTaskDelete(NULL);
}

// ============================================================================
// Public API
// ============================================================================

esp_err_t mqtt_client_init(void)
{
    if (g_initialized) {
        return ESP_OK;
    }

    // Load configuration from NVS
    esp_err_t ret = mqtt_client_load_config();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load configuration: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create message queue
    g_publish_queue = xQueueCreate(MQTT_QUEUE_SIZE, sizeof(mqtt_telemetry_t));
    if (g_publish_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create publish queue");
        return ESP_ERR_NO_MEM;
    }

    g_initialized = true;
    g_should_stop = false;

    ESP_LOGI(TAG, "MQTT client initialized");
    return ESP_OK;
}

esp_err_t mqtt_client_start(const app_mqtt_config_t *config)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_started) {
        return ESP_OK;
    }

    if (config != NULL) {
        g_config = *config;
        // Optionally save new config to NVS
        mqtt_client_save_config();
    }

    g_should_stop = false;
    g_mqtt_connected = false;

    SAFE_STRCPY(g_status.broker_url, g_config.broker_url, sizeof(g_status.broker_url));

    // Build full MQTT URI if not already a URL
    char uri[192];
    if (strstr(g_config.broker_url, "://") != NULL) {
        snprintf(uri, sizeof(uri), "%s", g_config.broker_url);
    } else {
        snprintf(uri, sizeof(uri), "mqtt://%s:%d", g_config.broker_url, g_config.port);
    }

    ESP_LOGI(TAG, "Connecting to %s", uri);

    // Configure ESP-MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
        .credentials.client_id = g_config.client_id,
        .credentials.username = (g_config.username[0] != '\0') ? g_config.username : NULL,
        .credentials.authentication.password = (g_config.password[0] != '\0') ? g_config.password : NULL,
        .session.keepalive = g_config.keep_alive,
        .network.reconnect_timeout_ms = MQTT_RECONNECT_DELAY_MS,
        .network.timeout_ms = MQTT_PUBLISH_TIMEOUT_MS,
        .network.disable_auto_reconnect = false,
    };

    g_mqtt_handle = esp_mqtt_client_init(&mqtt_cfg);
    if (g_mqtt_handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_ERR_NO_MEM;
    }

    // Register event handler
    esp_mqtt_client_register_event(g_mqtt_handle, ESP_EVENT_ANY_ID,
                                    mqtt_event_handler, NULL);

    // Start the MQTT client (handles connect/reconnect internally)
    esp_err_t ret = esp_mqtt_client_start(g_mqtt_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(g_mqtt_handle);
        g_mqtt_handle = NULL;
        return ret;
    }

    // Create dispatcher task for queued publishes
    BaseType_t task_ret = xTaskCreate(mqtt_dispatcher_task,
                                       "mqtt_dispatcher",
                                       MQTT_TASK_STACK_SIZE,
                                       NULL,
                                       MQTT_TASK_PRIORITY,
                                       &g_dispatcher_task);

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create dispatcher task");
        esp_mqtt_client_stop(g_mqtt_handle);
        esp_mqtt_client_destroy(g_mqtt_handle);
        g_mqtt_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    g_started = true;
    mqtt_client_update_state(MQTT_STATE_CONNECTING);

    ESP_LOGI(TAG, "MQTT client started (broker: %s:%d)",
             g_config.broker_url, g_config.port);
    return ESP_OK;
}

esp_err_t mqtt_client_get_config(app_mqtt_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *config = g_config;
    return ESP_OK;
}

esp_err_t mqtt_client_apply_config(const app_mqtt_config_t *config, bool restart_if_running)
{
    bool was_started;
    esp_err_t ret;

    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    g_config = *config;
    ret = mqtt_client_save_config();
    if (ret != ESP_OK) {
        return ret;
    }

    SAFE_STRCPY(g_status.broker_url, g_config.broker_url, sizeof(g_status.broker_url));

    was_started = g_started;
    if (restart_if_running && was_started) {
        ret = mqtt_client_stop();
        if (ret != ESP_OK) {
            return ret;
        }

        ret = mqtt_client_start(&g_config);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    ESP_LOGI(TAG, "MQTT config updated: broker=%s port=%u client_id=%s keep_alive=%u",
             g_config.broker_url,
             (unsigned int)g_config.port,
             g_config.client_id,
             (unsigned int)g_config.keep_alive);
    return ESP_OK;
}

esp_err_t mqtt_client_publish_telemetry(const mqtt_telemetry_t *telemetry)
{
    if (!g_started || g_publish_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (telemetry == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Queue the telemetry for publishing (non-blocking)
    if (xQueueSendToBack(g_publish_queue, (void *)telemetry, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Publish queue full, dropping message");
        return ESP_FAIL;
    }

    return ESP_OK;
}

mqtt_client_status_t mqtt_client_get_status(void)
{
    return g_status;
}

bool mqtt_client_is_connected(void)
{
    return g_mqtt_connected;
}

esp_err_t mqtt_client_stop(void)
{
    if (!g_started) {
        return ESP_OK;
    }

    g_should_stop = true;

    // Wait for dispatcher task to finish
    if (g_dispatcher_task != NULL) {
        if (mqtt_client_wait_dispatcher_stop(pdMS_TO_TICKS(MQTT_PUBLISH_TIMEOUT_MS)) != ESP_OK) {
            ESP_LOGW(TAG, "Dispatcher task stop timed out");
        }
    }

    // Stop and destroy MQTT client
    if (g_mqtt_handle != NULL) {
        esp_mqtt_client_stop(g_mqtt_handle);
        esp_mqtt_client_destroy(g_mqtt_handle);
        g_mqtt_handle = NULL;
    }

    if (g_publish_queue != NULL) {
        (void)xQueueReset(g_publish_queue);
    }

    g_mqtt_connected = false;
    mqtt_client_update_state(MQTT_STATE_DISCONNECTED);
    g_started = false;

    ESP_LOGI(TAG, "MQTT client stopped");
    return ESP_OK;
}

esp_err_t mqtt_client_deinit(void)
{
    if (!g_initialized) {
        return ESP_OK;
    }

    mqtt_client_stop();

    // Delete queue
    if (g_publish_queue != NULL) {
        vQueueDelete(g_publish_queue);
        g_publish_queue = NULL;
    }

    g_initialized = false;

    ESP_LOGI(TAG, "MQTT client deinitialized");
    return ESP_OK;
}
