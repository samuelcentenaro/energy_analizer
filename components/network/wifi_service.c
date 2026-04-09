/**
 * @file wifi_service.c
 * @brief Wi-Fi provisioning and station management service
 */

#include "wifi_service.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "app_mqtt_client.h"
#include "measurement_service.h"

static const char *TAG = "WIFI_SVC";

#define WIFI_NVS_NAMESPACE          "wifi_cfg"
#define WIFI_NVS_KEY_SSID           "ssid"
#define WIFI_NVS_KEY_PASS           "pass"
#define WIFI_PROVISION_AP_SSID      "EnergyAnalyzer-Setup"
#define WIFI_HTTP_FORM_BUFFER_SIZE  768U
#define WIFI_RETRY_BASE_DELAY_MS    1000U
#define WIFI_RETRY_MAX_DELAY_MS     30000U
#define WIFI_RETRY_TIMER_PERIOD_MS  1000U

static bool g_initialized = false;
static bool g_started = false;
static httpd_handle_t g_http_server = NULL;
static esp_netif_t *g_ap_netif = NULL;
static esp_netif_t *g_sta_netif = NULL;
static TimerHandle_t g_reconnect_timer = NULL;
static uint8_t g_retry_count = 0U;
static wifi_service_status_t g_status = {
    .state = WIFI_STATE_DISCONNECTED,
    .ap_active = false,
    .credentials_saved = false,
    .ap_ssid = WIFI_PROVISION_AP_SSID,
    .sta_ssid = {0},
    .ip_address = "0.0.0.0"
};
static app_wifi_config_t g_runtime_config = {0};

static esp_err_t wifi_service_open_nvs(nvs_handle_t *handle, nvs_open_mode_t mode);
static esp_err_t wifi_service_load_credentials(void);
static esp_err_t wifi_service_save_credentials(const app_wifi_config_t *config);
static esp_err_t wifi_service_configure_apsta(void);
static esp_err_t wifi_service_apply_sta_config(void);
static void wifi_service_update_state(wifi_state_t state);
static void wifi_service_clear_ip_address(void);
static esp_err_t wifi_service_start_http_server(void);
static esp_err_t wifi_service_html_get_handler(httpd_req_t *request);
static esp_err_t wifi_service_save_wifi_post_handler(httpd_req_t *request);
static esp_err_t wifi_service_save_mqtt_post_handler(httpd_req_t *request);
static esp_err_t wifi_service_save_calibration_post_handler(httpd_req_t *request);
static uint32_t wifi_service_get_retry_delay_ms(void);
static void wifi_service_reset_retry_backoff(void);
static esp_err_t wifi_service_schedule_retry(void);
static void wifi_service_retry_timer_callback(TimerHandle_t timer);
static void wifi_service_event_handler(void *arg,
                                       esp_event_base_t event_base,
                                       int32_t event_id,
                                       void *event_data);
static void wifi_service_url_decode(char *text);
static bool wifi_service_extract_form_value(const char *body,
                                            const char *key,
                                            char *output,
                                            size_t output_len);
static esp_err_t wifi_service_read_form_body(httpd_req_t *request,
                                             char *body,
                                             size_t body_size);
static const char *wifi_service_state_text(wifi_state_t state);

static const char *wifi_service_state_text(wifi_state_t state)
{
    switch (state) {
        case WIFI_STATE_PROVISIONING:
            return "PROVISIONING";
        case WIFI_STATE_CONNECTING:
            return "CONNECTING";
        case WIFI_STATE_CONNECTED:
            return "CONNECTED";
        case WIFI_STATE_ERROR:
            return "ERROR";
        default:
            return "DISCONNECTED";
    }
}

static esp_err_t wifi_service_open_nvs(nvs_handle_t *handle, nvs_open_mode_t mode)
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

    return nvs_open(WIFI_NVS_NAMESPACE, mode, handle);
}

static esp_err_t wifi_service_load_credentials(void)
{
    nvs_handle_t handle = 0;
    size_t ssid_len = sizeof(g_runtime_config.ssid);
    size_t password_len = sizeof(g_runtime_config.password);
    esp_err_t ret;

    memset(&g_runtime_config, 0, sizeof(g_runtime_config));

    ret = wifi_service_open_nvs(&handle, NVS_READONLY);
    if (ret != ESP_OK) {
        g_status.credentials_saved = false;
        return ret;
    }

    ret = nvs_get_str(handle, WIFI_NVS_KEY_SSID, g_runtime_config.ssid, &ssid_len);
    if (ret != ESP_OK) {
        nvs_close(handle);
        g_status.credentials_saved = false;
        return ret;
    }

    ret = nvs_get_str(handle, WIFI_NVS_KEY_PASS, g_runtime_config.password, &password_len);
    nvs_close(handle);
    if ((ret != ESP_OK) && (ret != ESP_ERR_NVS_NOT_FOUND)) {
        g_status.credentials_saved = false;
        return ret;
    }

    SAFE_STRCPY(g_status.sta_ssid, g_runtime_config.ssid, sizeof(g_status.sta_ssid));
    g_status.credentials_saved = (g_runtime_config.ssid[0] != '\0');

    return g_status.credentials_saved ? ESP_OK : ESP_ERR_NOT_FOUND;
}

static esp_err_t wifi_service_save_credentials(const app_wifi_config_t *config)
{
    nvs_handle_t handle = 0;
    esp_err_t ret;

    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = wifi_service_open_nvs(&handle, NVS_READWRITE);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_set_str(handle, WIFI_NVS_KEY_SSID, config->ssid);
    if (ret == ESP_OK) {
        ret = nvs_set_str(handle, WIFI_NVS_KEY_PASS, config->password);
    }
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }

    nvs_close(handle);

    if (ret == ESP_OK) {
        g_runtime_config = *config;
        SAFE_STRCPY(g_status.sta_ssid, g_runtime_config.ssid, sizeof(g_status.sta_ssid));
        g_status.credentials_saved = (g_runtime_config.ssid[0] != '\0');
    }

    return ret;
}

static void wifi_service_update_state(wifi_state_t state)
{
    if (g_status.state != state) {
        ESP_LOGI(TAG, "Wi-Fi state: %s -> %s",
                 wifi_service_state_text(g_status.state),
                 wifi_service_state_text(state));
        g_status.state = state;
        return;
    }

    g_status.state = state;
}

static void wifi_service_clear_ip_address(void)
{
    SAFE_STRCPY(g_status.ip_address, "0.0.0.0", sizeof(g_status.ip_address));
}

static esp_err_t wifi_service_apply_sta_config(void)
{
    wifi_config_t wifi_config = {0};
    esp_err_t ret;

    if (!g_status.credentials_saved) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&wifi_config, 0, sizeof(wifi_config));
    SAFE_STRCPY(wifi_config.sta.ssid, g_runtime_config.ssid, sizeof(wifi_config.sta.ssid));
    SAFE_STRCPY(wifi_config.sta.password, g_runtime_config.password, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        return ret;
    }

    wifi_service_clear_ip_address();
    wifi_service_update_state(WIFI_STATE_CONNECTING);
    ret = esp_wifi_connect();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi connection attempt started for SSID '%s'", g_runtime_config.ssid);
    }

    return ret;
}

static esp_err_t wifi_service_configure_apsta(void)
{
    wifi_config_t ap_config = {0};
    wifi_mode_t mode;
    esp_err_t ret;

    memset(&ap_config, 0, sizeof(ap_config));
    SAFE_STRCPY(ap_config.ap.ssid, WIFI_PROVISION_AP_SSID, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len = (uint8_t)strlen(WIFI_PROVISION_AP_SSID);
    ap_config.ap.channel = 1U;
    ap_config.ap.max_connection = 4U;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;

    mode = g_status.credentials_saved ? WIFI_MODE_APSTA : WIFI_MODE_AP;

    ret = esp_wifi_set_mode(mode);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        return ret;
    }

    g_status.ap_active = true;
    wifi_service_update_state(g_status.credentials_saved ? WIFI_STATE_CONNECTING : WIFI_STATE_PROVISIONING);

    if (g_status.credentials_saved) {
        ret = wifi_service_apply_sta_config();
        if (ret != ESP_OK) {
            wifi_service_update_state(WIFI_STATE_ERROR);
            return ret;
        }
    }

    return ESP_OK;
}

static void wifi_service_url_decode(char *text)
{
    char *source = text;
    char *target = text;

    if (text == NULL) {
        return;
    }

    while (*source != '\0') {
        if ((*source == '%') &&
            isxdigit((unsigned char)source[1]) &&
            isxdigit((unsigned char)source[2])) {
            char hex[3];

            hex[0] = source[1];
            hex[1] = source[2];
            hex[2] = '\0';
            *target = (char)strtol(hex, NULL, 16);
            source += 3;
        } else if (*source == '+') {
            *target = ' ';
            source++;
        } else {
            *target = *source;
            source++;
        }

        target++;
    }

    *target = '\0';
}

static bool wifi_service_extract_form_value(const char *body,
                                            const char *key,
                                            char *output,
                                            size_t output_len)
{
    const char *start;
    const char *end;
    size_t key_len;
    size_t value_len;

    if ((body == NULL) || (key == NULL) || (output == NULL) || (output_len == 0U)) {
        return false;
    }

    key_len = strlen(key);
    start = strstr(body, key);
    if (start == NULL) {
        output[0] = '\0';
        return false;
    }

    start += key_len;
    end = strchr(start, '&');
    if (end == NULL) {
        end = start + strlen(start);
    }

    value_len = (size_t)(end - start);
    if (value_len >= output_len) {
        value_len = output_len - 1U;
    }

    memcpy(output, start, value_len);
    output[value_len] = '\0';
    wifi_service_url_decode(output);
    return true;
}

static esp_err_t wifi_service_read_form_body(httpd_req_t *request,
                                             char *body,
                                             size_t body_size)
{
    int received = 0;

    if ((request == NULL) || (body == NULL) || (body_size == 0U)) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(body, 0, body_size);

    if (request->content_len >= (int)body_size) {
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Form too large");
        return ESP_ERR_INVALID_SIZE;
    }

    while (received < request->content_len) {
        int chunk = httpd_req_recv(request, &body[received], (size_t)(request->content_len - received));
        if (chunk <= 0) {
            httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read form");
            return ESP_FAIL;
        }

        received += chunk;
    }

    return ESP_OK;
}

static esp_err_t wifi_service_html_get_handler(httpd_req_t *request)
{
    char line[384];
    const char *state_text;
    app_mqtt_config_t mqtt_config = {0};
    field_calibration_t calibration = {0};
    adc_calibration_t active_calibration = {0};

    switch (g_status.state) {
        case WIFI_STATE_PROVISIONING:
            state_text = "Provisioning AP active";
            break;
        case WIFI_STATE_CONNECTING:
            state_text = "Connecting to Wi-Fi";
            break;
        case WIFI_STATE_CONNECTED:
            state_text = "Connected";
            break;
        case WIFI_STATE_ERROR:
            state_text = "Connection error";
            break;
        default:
            state_text = "Idle";
            break;
    }

    (void)mqtt_client_get_config(&mqtt_config);
    (void)measurement_service_get_field_calibration(&calibration);
    (void)measurement_service_get_calibration(&active_calibration);
    httpd_resp_set_type(request, "text/html");
    if (httpd_resp_sendstr_chunk(request,
                                 "<!doctype html><html><head><meta charset=\"utf-8\">"
                                 "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                                 "<title>Energy Analyzer Config</title>"
                                 "<style>body{font-family:Arial,sans-serif;max-width:720px;margin:24px auto;padding:16px;background:#f5f7f7;color:#1f2a2a;}"
                                 "input{width:100%;padding:12px;margin:8px 0;box-sizing:border-box;border:1px solid #c7d0d0;border-radius:6px;}"
                                 "button{width:100%;padding:12px;background:#0a7d5a;color:#fff;border:0;border-radius:6px;}"
                                 ".card{border:1px solid #d7dddd;padding:16px;border-radius:10px;background:#fff;margin-bottom:16px;}"
                                 "h1,h2{margin-top:0;}small{color:#667;}</style></head><body><h1>Energy Analyzer Config</h1>") != ESP_OK) {
        return ESP_FAIL;
    }

    (void)snprintf(line, sizeof(line),
                   "<div class=\"card\"><h2>Wi-Fi</h2>"
                   "<p>AP SSID: <strong>%s</strong></p>"
                   "<p>State: <strong>%s</strong></p>"
                   "<p>Current SSID: <strong>%s</strong></p>"
                   "<p>IP: <strong>%s</strong></p>",
                   g_status.ap_ssid,
                   state_text,
                   g_status.credentials_saved ? g_status.sta_ssid : "-",
                   g_status.ip_address);
    if (httpd_resp_sendstr_chunk(request, line) != ESP_OK) {
        return ESP_FAIL;
    }
    (void)snprintf(line, sizeof(line),
                   "<form method=\"post\" action=\"/save_wifi\">"
                   "<label>SSID</label><input name=\"ssid\" maxlength=\"31\" value=\"%s\">"
                   "<label>Password</label><input name=\"password\" type=\"password\" maxlength=\"63\">"
                   "<button type=\"submit\">Save and Connect</button></form></div>",
                   g_status.credentials_saved ? g_status.sta_ssid : "");
    if (httpd_resp_sendstr_chunk(request, line) != ESP_OK) {
        return ESP_FAIL;
    }

    (void)snprintf(line, sizeof(line),
                   "<div class=\"card\"><h2>MQTT</h2>"
                   "<p>Broker now: <strong>%s:%u</strong></p>",
                   mqtt_config.broker_url,
                   (unsigned int)mqtt_config.port);
    if (httpd_resp_sendstr_chunk(request, line) != ESP_OK) {
        return ESP_FAIL;
    }
    (void)snprintf(line, sizeof(line),
                   "<form method=\"post\" action=\"/save_mqtt\">"
                   "<label>Broker host, IP, or URL</label><input name=\"broker\" maxlength=\"127\" value=\"%s\">"
                   "<label>Port</label><input name=\"port\" maxlength=\"5\" value=\"%u\">",
                   mqtt_config.broker_url,
                   (unsigned int)mqtt_config.port);
    if (httpd_resp_sendstr_chunk(request, line) != ESP_OK) {
        return ESP_FAIL;
    }
    (void)snprintf(line, sizeof(line),
                   "<label>Client ID</label><input name=\"client_id\" maxlength=\"31\" value=\"%s\">"
                   "<label>Keep Alive</label><input name=\"keep_alive\" maxlength=\"3\" value=\"%u\">"
                   "<button type=\"submit\">Save MQTT Config</button></form>",
                   mqtt_config.client_id,
                   (unsigned int)mqtt_config.keep_alive);
    if (httpd_resp_sendstr_chunk(request, line) != ESP_OK) {
        return ESP_FAIL;
    }
    if (httpd_resp_sendstr_chunk(request,
                                 "<small>No authentication in this version. Restrict access at network level.</small></div>") != ESP_OK) {
        return ESP_FAIL;
    }

    (void)snprintf(line, sizeof(line),
                   "<div class=\"card\"><h2>Calibration</h2>"
                   "<p>Active V scale: <strong>%.3f</strong> | V offset: <strong>%.3f</strong></p>"
                   "<p>Active I scale: <strong>%.3f</strong> | I offset: <strong>%.3f</strong></p>"
                   "<p>State: <strong>%d</strong></p>",
                   active_calibration.voltage_scale,
                   active_calibration.voltage_offset,
                   active_calibration.current_scale,
                   active_calibration.current_offset,
                   (int)calibration.state);
    if (httpd_resp_sendstr_chunk(request, line) != ESP_OK) {
        return ESP_FAIL;
    }

    (void)snprintf(line, sizeof(line),
                   "<form method=\"post\" action=\"/save_calibration\">"
                   "<label>Voltage Scale</label><input name=\"v_scale\" value=\"%.3f\">"
                   "<label>Voltage Offset</label><input name=\"v_offset\" value=\"%.3f\">",
                   active_calibration.voltage_scale,
                   active_calibration.voltage_offset);
    if (httpd_resp_sendstr_chunk(request, line) != ESP_OK) {
        return ESP_FAIL;
    }
    (void)snprintf(line, sizeof(line),
                   "<label>Current Scale</label><input name=\"i_scale\" value=\"%.3f\">"
                   "<label>Current Offset</label><input name=\"i_offset\" value=\"%.3f\">"
                   "<button type=\"submit\">Save Calibration</button>"
                   "</form></div></body></html>",
                   active_calibration.current_scale,
                   active_calibration.current_offset);
    if (httpd_resp_sendstr_chunk(request, line) != ESP_OK) {
        return ESP_FAIL;
    }

    return httpd_resp_sendstr_chunk(request, NULL);
}

static esp_err_t wifi_service_save_wifi_post_handler(httpd_req_t *request)
{
    char body[WIFI_HTTP_FORM_BUFFER_SIZE];
    app_wifi_config_t config = {0};
    esp_err_t ret;

    ret = wifi_service_read_form_body(request, body, sizeof(body));
    if (ret != ESP_OK) {
        return ret;
    }

    if (!wifi_service_extract_form_value(body, "ssid=", config.ssid, sizeof(config.ssid))) {
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "SSID missing");
        return ESP_FAIL;
    }

    (void)wifi_service_extract_form_value(body, "password=", config.password, sizeof(config.password));

    ret = wifi_service_save_credentials(&config);
    if (ret != ESP_OK) {
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save credentials");
        return ret;
    }

    if (esp_wifi_set_mode(WIFI_MODE_APSTA) == ESP_OK) {
        ret = wifi_service_apply_sta_config();
    }

    if (ret != ESP_OK) {
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Saved, but connect failed");
        return ret;
    }

    ESP_LOGI(TAG, "Wi-Fi credentials updated for SSID '%s'", g_status.sta_ssid);
    httpd_resp_set_type(request, "text/html");
    return httpd_resp_sendstr(request,
                              "<html><body><h1>Saved</h1>"
                              "<p>Credentials stored. The device is connecting now.</p>"
                              "<p>Reload the page in a few seconds to check IP status.</p>"
                              "</body></html>");
}

static esp_err_t wifi_service_save_mqtt_post_handler(httpd_req_t *request)
{
    char body[WIFI_HTTP_FORM_BUFFER_SIZE];
    char value[32];
    app_mqtt_config_t config = {0};
    esp_err_t ret;
    mqtt_client_status_t mqtt_status;

    ret = wifi_service_read_form_body(request, body, sizeof(body));
    if (ret != ESP_OK) {
        return ret;
    }

    ret = mqtt_client_get_config(&config);
    if (ret != ESP_OK) {
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "MQTT config unavailable");
        return ret;
    }

    if (!wifi_service_extract_form_value(body, "broker=", config.broker_url, sizeof(config.broker_url))) {
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Broker missing");
        return ESP_FAIL;
    }

    if (wifi_service_extract_form_value(body, "port=", value, sizeof(value))) {
        unsigned long port = strtoul(value, NULL, 10);
        if ((port == 0UL) || (port > 65535UL)) {
            httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid port");
            return ESP_ERR_INVALID_ARG;
        }
        config.port = (uint16_t)port;
    }

    if (wifi_service_extract_form_value(body, "client_id=", config.client_id, sizeof(config.client_id))) {
        if (config.client_id[0] == '\0') {
            httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Client ID missing");
            return ESP_ERR_INVALID_ARG;
        }
    }

    if (wifi_service_extract_form_value(body, "keep_alive=", value, sizeof(value))) {
        unsigned long keep_alive = strtoul(value, NULL, 10);
        if ((keep_alive < 15UL) || (keep_alive > 300UL)) {
            httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid keep alive");
            return ESP_ERR_INVALID_ARG;
        }
        config.keep_alive = (uint16_t)keep_alive;
    }

    mqtt_status = mqtt_client_get_status();

    ret = mqtt_client_apply_config(&config, true);
    if (ret != ESP_OK) {
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save MQTT config");
        return ret;
    }

    ESP_LOGI(TAG, "MQTT config updated from web portal (prev_state=%d)", (int)mqtt_status.state);
    httpd_resp_set_type(request, "text/html");
    return httpd_resp_sendstr(request,
                              "<html><body><h1>MQTT Saved</h1>"
                              "<p>MQTT configuration stored successfully.</p>"
                              "<p>Reload the page to inspect the current broker and status.</p>"
                              "</body></html>");
}

static esp_err_t wifi_service_save_calibration_post_handler(httpd_req_t *request)
{
    char body[WIFI_HTTP_FORM_BUFFER_SIZE];
    char value[32];
    adc_calibration_t calibration = {0};
    esp_err_t ret;

    ret = wifi_service_read_form_body(request, body, sizeof(body));
    if (ret != ESP_OK) {
        return ret;
    }

    ret = measurement_service_get_calibration(&calibration);
    if (ret != ESP_OK) {
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Calibration unavailable");
        return ret;
    }

    if (wifi_service_extract_form_value(body, "v_scale=", value, sizeof(value))) {
        calibration.voltage_scale = strtof(value, NULL);
    }
    if (wifi_service_extract_form_value(body, "v_offset=", value, sizeof(value))) {
        calibration.voltage_offset = strtof(value, NULL);
    }
    if (wifi_service_extract_form_value(body, "i_scale=", value, sizeof(value))) {
        calibration.current_scale = strtof(value, NULL);
    }
    if (wifi_service_extract_form_value(body, "i_offset=", value, sizeof(value))) {
        calibration.current_offset = strtof(value, NULL);
    }

    ret = measurement_service_set_calibration(&calibration);
    if (ret == ESP_OK) {
        ret = measurement_service_save_calibration();
    }
    if (ret != ESP_OK) {
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save calibration");
        return ret;
    }

    ESP_LOGI(TAG, "Calibration updated from web portal");
    httpd_resp_set_type(request, "text/html");
    return httpd_resp_sendstr(request,
                              "<html><body><h1>Calibration Saved</h1>"
                              "<p>Calibration values were stored successfully.</p>"
                              "<p>Reload the page to confirm the active coefficients.</p>"
                              "</body></html>");
}

static esp_err_t wifi_service_start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = wifi_service_html_get_handler,
        .user_ctx = NULL
    };
    httpd_uri_t save_wifi_uri = {
        .uri = "/save_wifi",
        .method = HTTP_POST,
        .handler = wifi_service_save_wifi_post_handler,
        .user_ctx = NULL
    };
    httpd_uri_t save_mqtt_uri = {
        .uri = "/save_mqtt",
        .method = HTTP_POST,
        .handler = wifi_service_save_mqtt_post_handler,
        .user_ctx = NULL
    };
    httpd_uri_t save_calibration_uri = {
        .uri = "/save_calibration",
        .method = HTTP_POST,
        .handler = wifi_service_save_calibration_post_handler,
        .user_ctx = NULL
    };
    esp_err_t ret;

    config.stack_size = 8192U;

    if (g_http_server != NULL) {
        return ESP_OK;
    }

    ret = httpd_start(&g_http_server, &config);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = httpd_register_uri_handler(g_http_server, &root_uri);
    if (ret == ESP_OK) {
        ret = httpd_register_uri_handler(g_http_server, &save_wifi_uri);
    }
    if (ret == ESP_OK) {
        ret = httpd_register_uri_handler(g_http_server, &save_mqtt_uri);
    }
    if (ret == ESP_OK) {
        ret = httpd_register_uri_handler(g_http_server, &save_calibration_uri);
    }

    if (ret != ESP_OK) {
        httpd_stop(g_http_server);
        g_http_server = NULL;
    }

    return ret;
}

static uint32_t wifi_service_get_retry_delay_ms(void)
{
    uint32_t delay_ms = WIFI_RETRY_BASE_DELAY_MS;
    uint8_t steps = g_retry_count;

    while ((steps > 0U) && (delay_ms < WIFI_RETRY_MAX_DELAY_MS)) {
        if (delay_ms > (WIFI_RETRY_MAX_DELAY_MS / 2U)) {
            delay_ms = WIFI_RETRY_MAX_DELAY_MS;
        } else {
            delay_ms *= 2U;
        }
        steps--;
    }

    if (delay_ms > WIFI_RETRY_MAX_DELAY_MS) {
        delay_ms = WIFI_RETRY_MAX_DELAY_MS;
    }

    return delay_ms;
}

static void wifi_service_reset_retry_backoff(void)
{
    g_retry_count = 0U;

    if (g_reconnect_timer != NULL) {
        (void)xTimerStop(g_reconnect_timer, 0U);
    }
}

static esp_err_t wifi_service_schedule_retry(void)
{
    uint32_t retry_delay_ms;
    BaseType_t timer_ret;

    if (!g_status.credentials_saved) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_reconnect_timer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    retry_delay_ms = wifi_service_get_retry_delay_ms();
    timer_ret = xTimerChangePeriod(g_reconnect_timer,
                                   pdMS_TO_TICKS(retry_delay_ms),
                                   0U);
    if (timer_ret != pdPASS) {
        return ESP_FAIL;
    }

    timer_ret = xTimerStart(g_reconnect_timer, 0U);
    if (timer_ret != pdPASS) {
        return ESP_FAIL;
    }

    if (g_retry_count < 8U) {
        g_retry_count++;
    }

    ESP_LOGI(TAG, "Wi-Fi reconnect scheduled in %lu ms (attempt=%u)",
             (unsigned long)retry_delay_ms,
             (unsigned int)g_retry_count);
    return ESP_OK;
}

static void wifi_service_retry_timer_callback(TimerHandle_t timer)
{
    esp_err_t ret;

    (void)timer;

    if (!g_started || !g_status.credentials_saved) {
        return;
    }

    wifi_service_update_state(WIFI_STATE_CONNECTING);
    ret = wifi_service_apply_sta_config();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Deferred Wi-Fi reconnect failed: 0x%02x", ret);
        (void)wifi_service_schedule_retry();
    }
}

static void wifi_service_event_handler(void *arg,
                                       esp_event_base_t event_base,
                                       int32_t event_id,
                                       void *event_data)
{
    (void)arg;

    if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_START)) {
        ESP_LOGI(TAG, "STA start requested");
        return;
    }

    if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_DISCONNECTED)) {
        wifi_service_clear_ip_address();

        if (g_status.credentials_saved) {
            wifi_service_update_state(WIFI_STATE_CONNECTING);
            if (wifi_service_schedule_retry() != ESP_OK) {
                ESP_LOGW(TAG, "Wi-Fi disconnected, immediate retry fallback");
                (void)esp_wifi_connect();
            }
        } else {
            wifi_service_update_state(WIFI_STATE_PROVISIONING);
        }

        return;
    }

    if ((event_base == IP_EVENT) && (event_id == IP_EVENT_STA_GOT_IP)) {
        ip_event_got_ip_t *ip_event = (ip_event_got_ip_t *)event_data;

        if (ip_event != NULL) {
            (void)snprintf(g_status.ip_address,
                           sizeof(g_status.ip_address),
                           IPSTR,
                           IP2STR(&ip_event->ip_info.ip));
        }

        wifi_service_reset_retry_backoff();
        wifi_service_update_state(WIFI_STATE_CONNECTED);
        ESP_LOGI(TAG, "Wi-Fi connected, IP=%s", g_status.ip_address);
    }
}

esp_err_t wifi_service_init(void)
{
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret;

    if (g_initialized) {
        return ESP_OK;
    }

    ret = esp_netif_init();
    if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
        return ret;
    }

    ret = esp_event_loop_create_default();
    if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
        return ret;
    }

    g_ap_netif = esp_netif_create_default_wifi_ap();
    g_sta_netif = esp_netif_create_default_wifi_sta();
    if ((g_ap_netif == NULL) || (g_sta_netif == NULL)) {
        return ESP_FAIL;
    }

    g_reconnect_timer = xTimerCreate("wifi_retry",
                                     pdMS_TO_TICKS(WIFI_RETRY_TIMER_PERIOD_MS),
                                     pdFALSE,
                                     NULL,
                                     wifi_service_retry_timer_callback);
    if (g_reconnect_timer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ret = esp_wifi_init(&wifi_init_config);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_service_event_handler, NULL);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_service_event_handler, NULL);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (ret != ESP_OK) {
        return ret;
    }

    (void)wifi_service_load_credentials();
    g_initialized = true;

    ESP_LOGI(TAG, "Wi-Fi service initialized");
    return ESP_OK;
}

esp_err_t wifi_service_start(void)
{
    esp_err_t ret;

    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_started) {
        return ESP_OK;
    }

    ret = wifi_service_configure_apsta();
    if (ret != ESP_OK) {
        wifi_service_update_state(WIFI_STATE_ERROR);
        return ret;
    }

    ret = wifi_service_start_http_server();
    if (ret != ESP_OK) {
        wifi_service_update_state(WIFI_STATE_ERROR);
        return ret;
    }

    g_started = true;

    ESP_LOGI(TAG, "Provisioning AP active: SSID='%s'", g_status.ap_ssid);
    ESP_LOGI(TAG, "Open http://192.168.4.1 to configure Wi-Fi");
    if (g_status.credentials_saved) {
        ESP_LOGI(TAG, "Stored Wi-Fi SSID found: '%s'", g_status.sta_ssid);
    } else {
        ESP_LOGI(TAG, "No stored Wi-Fi credentials yet");
    }

    return ESP_OK;
}

esp_err_t wifi_service_get_status(wifi_service_status_t *status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *status = g_status;
    return ESP_OK;
}

esp_err_t wifi_service_clear_credentials(void)
{
    nvs_handle_t handle = 0;
    esp_err_t ret;

    ret = wifi_service_open_nvs(&handle, NVS_READWRITE);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_erase_key(handle, WIFI_NVS_KEY_SSID);
    if ((ret == ESP_OK) || (ret == ESP_ERR_NVS_NOT_FOUND)) {
        ret = nvs_erase_key(handle, WIFI_NVS_KEY_PASS);
    }
    if ((ret == ESP_OK) || (ret == ESP_ERR_NVS_NOT_FOUND)) {
        ret = nvs_commit(handle);
    }

    nvs_close(handle);

    memset(&g_runtime_config, 0, sizeof(g_runtime_config));
    memset(g_status.sta_ssid, 0, sizeof(g_status.sta_ssid));
    g_status.credentials_saved = false;
    wifi_service_clear_ip_address();
    wifi_service_reset_retry_backoff();
    wifi_service_update_state(WIFI_STATE_PROVISIONING);

    if (g_started) {
        (void)esp_wifi_disconnect();
        (void)esp_wifi_set_mode(WIFI_MODE_AP);
    }

    ESP_LOGI(TAG, "Stored Wi-Fi credentials cleared");
    return ((ret == ESP_OK) || (ret == ESP_ERR_NVS_NOT_FOUND)) ? ESP_OK : ret;
}
