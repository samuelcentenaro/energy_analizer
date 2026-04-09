/**
 * @file wifi_service.h
 * @brief Wi-Fi provisioning and connection service
 */

#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "common_types.h"

#define WIFI_SERVICE_AP_SSID_MAX_LEN 32U
#define WIFI_SERVICE_IP_ADDR_MAX_LEN 16U

typedef struct {
    wifi_state_t state;
    bool ap_active;
    bool credentials_saved;
    char ap_ssid[WIFI_SERVICE_AP_SSID_MAX_LEN + 1U];
    char sta_ssid[sizeof(((app_wifi_config_t *)0)->ssid)];
    char ip_address[WIFI_SERVICE_IP_ADDR_MAX_LEN];
} wifi_service_status_t;

esp_err_t wifi_service_init(void);
esp_err_t wifi_service_start(void);
esp_err_t wifi_service_get_status(wifi_service_status_t *status);
esp_err_t wifi_service_clear_credentials(void);

#endif // WIFI_SERVICE_H
