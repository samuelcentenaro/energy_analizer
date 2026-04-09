/**
 * @file network.h
 * @brief Network layer umbrella header
 *
 * This layer contains the active connectivity services used by the production
 * firmware baseline.
 *
 * Active components:
 * - wifi_service.h
 * - app_mqtt_client.h
 *
 * Reserved future areas:
 * - TLS hardening
 * - OTA transport and update orchestration
 * - remote management services
 */

#ifndef NETWORK_H
#define NETWORK_H

#include "esp_err.h"

#include "wifi_service.h"
#include "app_mqtt_client.h"

#endif // NETWORK_H
