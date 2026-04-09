#ifndef FEATURE_CONFIG_H
#define FEATURE_CONFIG_H

/**
 * @file feature_config.h
 * @brief Feature flags and compile-time options for Energy Analyzer
 * 
 * This file contains preprocessor flags that enable/disable features
 * at compile time, allowing for different firmware variants.
 * 
 * @note Use these flags to customize the firmware for different use cases
 * @note Some flags may affect memory usage or performance
 */

// ============================================================================
// Core Features (Always Enabled for Energy Analyzer)
// ============================================================================

/// @brief Enable legacy internal ADC sensor path (deprecated, keep disabled)
#define FEATURE_ADC_SENSOR       0

/// @brief Enable OLED display
#define FEATURE_OLED_DISPLAY     1

/// @brief Enable button interface
/// @note Implemented in firmware, but hardware validation is still pending
#define FEATURE_BUTTON_INTERFACE 1

// ============================================================================
// Communication Features
// ============================================================================

/// @brief Enable WiFi connectivity
#define FEATURE_WIFI             1

/// @brief Enable MQTT publishing
#define FEATURE_MQTT             1

/// @brief Enable UART debug output
#define FEATURE_DEBUG_SERIAL     1

// ============================================================================
// Analysis Features
// ============================================================================

/// @brief Enable RMS calculation
#define FEATURE_RMS_CALCULATION  1

/// @brief Enable power factor calculation
#define FEATURE_POWER_CALCULATION 1

/// @brief Enable harmonic analysis
/// @note Future feature only, keep disabled in current production baseline
#define FEATURE_HARMONIC_ANALYSIS 0

/// @brief Enable SAG/SWELL detection
/// @note Future feature only, keep disabled in current production baseline
#define FEATURE_SAG_SWELL_DETECTION 0

/// @brief Enable flicker measurement
/// @note Future feature only, keep disabled in current production baseline
#define FEATURE_FLICKER_MEASUREMENT 0

// ============================================================================
// Storage Features
// ============================================================================

/// @brief Enable NVS calibration storage
#define FEATURE_NVS_CALIBRATION  1

/// @brief Enable configuration persistence
#define FEATURE_CONFIG_PERSISTENCE 1

// ============================================================================
// Debug and Development Features
// ============================================================================

/// @brief Enable verbose logging (development only)
#define FEATURE_VERBOSE_LOGGING  0

/// @brief Enable performance profiling
#define FEATURE_PERFORMANCE_PROFILING 0

/// @brief Enable memory debugging
#define FEATURE_MEMORY_DEBUGGING 0

/// @brief Enable test functions
#define FEATURE_UNIT_TESTS       0

// ============================================================================
// Hardware Variants
// ============================================================================

/// @brief Use the production external ADC path (ADS1015 family)
#define FEATURE_EXTERNAL_ADC     1

/// @brief Use different OLED controller (SH1106 instead of SSD1306)
#define FEATURE_SH1106_OLED      0

/// @brief Enable battery monitoring
#define FEATURE_BATTERY_MONITOR  0

// ============================================================================
// Optimization Flags
// ============================================================================

/// @brief Use DMA for ADC transfers (ESP32-S3+ only)
#define FEATURE_ADC_DMA          0

/// @brief Enable light sleep between measurements
#define FEATURE_LIGHT_SLEEP      0

/// @brief Use static memory allocation only (no malloc)
#define FEATURE_STATIC_ALLOCATION_ONLY 1

// ============================================================================
// Safety and Reliability Features
// ============================================================================

/// @brief Enable watchdog timer
#define FEATURE_WATCHDOG         1

/// @brief Enable brownout detection
#define FEATURE_BROWNOUT_DETECT  1

/// @brief Enable input validation on all APIs
#define FEATURE_INPUT_VALIDATION 1

/// @brief Enable error recovery mechanisms
#define FEATURE_ERROR_RECOVERY   1

#endif // FEATURE_CONFIG_H
