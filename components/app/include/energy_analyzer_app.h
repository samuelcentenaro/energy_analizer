/**
 * @file energy_analyzer_app.h
 * @brief Main application controller for Energy Analyzer
 *
 * This module provides the high-level application logic and system orchestration.
 * It coordinates between different layers and manages the overall system behavior.
 */

#ifndef ENERGY_ANALYZER_APP_H
#define ENERGY_ANALYZER_APP_H

#include "esp_err.h"

/**
 * @brief Initialize the Energy Analyzer application
 *
 * This function initializes the active production path in the correct order:
 * 1. Shared I2C and board HAL
 * 2. ADS1015 acquisition path
 * 3. Measurement service
 * 4. Network services
 *
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t energy_analyzer_app_init(void);

/**
 * @brief Start the main application tasks
 *
 * Starts the runtime services needed by the production firmware, including
 * Wi-Fi provisioning/connection and deferred MQTT runtime control.
 *
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t energy_analyzer_app_start(void);

/**
 * @brief Main application loop (for simple implementations)
 *
 * This function contains the cooperative production loop used by the current
 * firmware baseline for acquisition, UI refresh, health checks, and telemetry.
 */
void energy_analyzer_app_run(void);

#endif // ENERGY_ANALYZER_APP_H
