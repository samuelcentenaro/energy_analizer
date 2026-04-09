#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

/**
 * @file common_types.h
 * @brief Common data types and structures for Energy Analyzer
 * 
 * This file defines all shared data structures, enumerations, and
 * type definitions used across multiple components.
 * 
 * @note All structures are packed for memory efficiency
 * @note Error codes follow ESP-IDF conventions
 */

// ============================================================================
// Standard Includes
// ============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// ============================================================================
// Error Codes (ESP-IDF compatible)
// ============================================================================

/// @brief Custom error codes for Energy Analyzer
typedef enum {
    // Success
    ERR_OK = 0x00,
    
    // Parameter errors
    ERR_INVALID_PARAM = 0x01,
    ERR_NULL_POINTER = 0x02,
    ERR_OUT_OF_RANGE = 0x03,
    
    // State errors
    ERR_NOT_INITIALIZED = 0x04,
    ERR_ALREADY_INITIALIZED = 0x05,
    ERR_BUSY = 0x06,
    
    // Hardware errors
    ERR_HW_FAILURE = 0x07,
    ERR_TIMEOUT = 0x08,
    ERR_SENSOR_FAILURE = 0x09,
    
    // Communication errors
    ERR_COMM_FAILURE = 0x0A,
    ERR_I2C_FAILURE = 0x0B,
    
    // Memory errors
    ERR_NO_MEMORY = 0x0C,
    ERR_BUFFER_OVERFLOW = 0x0D,
    
    // Configuration errors
    ERR_INVALID_CONFIG = 0x0E,
    ERR_CALIBRATION_INVALID = 0x0F
} error_code_t;

// ============================================================================
// Measurement Data Structures
// ============================================================================

/// @brief ADC reading structure
typedef struct {
    uint32_t voltage_raw;      ///< Raw ADC value for voltage (0-4095)
    uint32_t current_raw;      ///< Raw ADC value for current (0-4095)
    uint32_t timestamp_ms;     ///< Timestamp in milliseconds
} adc_reading_t;

/// @brief RMS measurement results
typedef struct {
    float voltage_rms;         ///< RMS voltage in Volts
    float current_rms;         ///< RMS current in Amperes
    float timestamp_s;         ///< Timestamp in seconds
} rms_measurement_t;

/// @brief Complete measurement result structure
typedef struct {
    float voltage_rms;         ///< RMS voltage in Volts
    float current_rms;         ///< RMS current in Amperes
    float power_real;          ///< Real power (Watts)
    float power_reactive;      ///< Reactive power (VAR)
    float power_apparent;      ///< Apparent power (VA)
    float power_factor;        ///< Power factor (0-1)
    uint32_t sample_count;     ///< Number of samples used
    uint32_t timestamp_ms;     ///< Timestamp in milliseconds
} measurement_result_t;

/// @brief Measurement statistics
typedef struct {
    uint32_t total_samples;    ///< Total samples processed
    uint32_t valid_measurements; ///< Valid measurements completed
    uint32_t errors_count;     ///< Number of processing errors
    float avg_voltage;         ///< Average voltage over time
    float avg_current;         ///< Average current over time
} measurement_stats_t;

/// @brief Quality metrics for power analysis
typedef struct {
    float frequency;           ///< Grid frequency (Hz)
    float thd_percent;         ///< Total Harmonic Distortion (%)
    float harmonics[19];       ///< Odd harmonics 3,5,7,...,37 (%)
    float sag_depth;           ///< Voltage sag depth (pu)
    float swell_depth;         ///< Voltage swell depth (pu)
    float flicker_pst;         ///< Short-term flicker (Pst)
    float flicker_plt;         ///< Long-term flicker (Plt)
} quality_metrics_t;

// ============================================================================
// Calibration Structures
// ============================================================================

/// @brief ADC calibration parameters
typedef struct {
    float voltage_offset;      ///< Voltage offset correction
    float voltage_scale;       ///< Voltage scale factor
    float current_offset;      ///< Current offset correction
    float current_scale;       ///< Current scale factor
} adc_calibration_t;

/// @brief Field calibration workflow state
typedef enum {
    CALIBRATION_STATE_FACTORY_DEFAULT = 0,
    CALIBRATION_STATE_PENDING,
    CALIBRATION_STATE_VALIDATED,
    CALIBRATION_STATE_APPLIED
} calibration_state_t;

/// @brief Reference values captured during field calibration
typedef struct {
    float reference_voltage_rms;   ///< External reference RMS voltage
    float measured_voltage_rms;    ///< Measured RMS voltage before correction
    float reference_current_rms;   ///< External reference RMS current
    float measured_current_rms;    ///< Measured RMS current before correction
} calibration_reference_t;

/// @brief Full field calibration record
typedef struct {
    adc_calibration_t coefficients;            ///< Active coefficients to apply
    calibration_reference_t reference;         ///< Bench reference used to derive coefficients
    calibration_state_t state;                 ///< Current calibration state
    uint32_t updated_at_ms;                    ///< Timestamp of last update
    bool has_voltage_reference;                ///< True when voltage reference is populated
    bool has_current_reference;                ///< True when current reference is populated
} field_calibration_t;

// ============================================================================
// Component State Enumerations
// ============================================================================

/// @brief Component initialization states
typedef enum {
    COMPONENT_STATE_UNINITIALIZED = 0,
    COMPONENT_STATE_INITIALIZING,
    COMPONENT_STATE_READY,
    COMPONENT_STATE_ERROR,
    COMPONENT_STATE_SHUTDOWN
} component_state_t;

/// @brief Sensor status
typedef enum {
    SENSOR_STATUS_OK = 0,
    SENSOR_STATUS_DISCONNECTED,
    SENSOR_STATUS_OUT_OF_RANGE,
    SENSOR_STATUS_CALIBRATION_NEEDED,
    SENSOR_STATUS_HARDWARE_ERROR
} sensor_status_t;

/// @brief WiFi connection states
typedef enum {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_PROVISIONING,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_ERROR
} wifi_state_t;

/// @brief MQTT connection states
typedef enum {
    MQTT_STATE_DISCONNECTED = 0,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_ERROR
} mqtt_state_t;

// ============================================================================
// Configuration Structures
// ============================================================================

/// @brief WiFi configuration
typedef struct {
    char ssid[32];             ///< WiFi SSID
    char password[64];         ///< WiFi password
    uint8_t security;          ///< Security type (WPA2, etc.)
} app_wifi_config_t;

/// @brief MQTT configuration
typedef struct {
    char broker_url[128];      ///< MQTT broker URL
    uint16_t port;             ///< MQTT port (usually 1883)
    char client_id[32];        ///< Client ID
    char username[32];         ///< Username (optional)
    char password[32];         ///< Password (optional)
    uint16_t keep_alive;       ///< Keep-alive interval (seconds)
} app_mqtt_config_t;

// ============================================================================
// Utility Macros
// ============================================================================

/// @brief Safe string copy with null termination
/// @note Works with both char arrays and uint8_t arrays
#define SAFE_STRCPY(dest, src, size) do { \
    strncpy((char *)(dest), (src), (size) - 1); \
    ((char *)(dest))[(size) - 1] = '\0'; \
} while(0)


/// @brief Convert error_code_t to esp_err_t
#define ERROR_TO_ESP_ERR(code) ((esp_err_t)(code + 0x1000))

/// @brief Check if value is within range
#define IN_RANGE(value, min, max) ((value) >= (min) && (value) <= (max))

#endif // COMMON_TYPES_H
