#ifndef TIMING_CONFIG_H
#define TIMING_CONFIG_H

/**
 * @file timing_config.h
 * @brief Timing and frequency configuration for Energy Analyzer
 * 
 * This file contains all timing-related constants including sampling
 * rates, timeouts, and delays used throughout the system.
 * 
 * @note All times are in milliseconds unless specified
 * @note Modify these values based on performance requirements
 */

// ============================================================================
// ADC Sampling Configuration
// ============================================================================

/// @brief ADC sampling frequency (Hz) - 8 kHz for good resolution
#define ADC_SAMPLING_FREQUENCY_HZ       8000

/// @brief RMS calculation window size (samples) - ~500ms at 8kHz
#define ADC_RMS_WINDOW_SAMPLES          4000

/// @brief ADC read timeout (ms) - should be fast
#define ADC_READ_TIMEOUT_MS             50

/// @brief ADC initialization timeout (ms)
#define ADC_INIT_TIMEOUT_MS             1000

// ============================================================================
// Measurement Configuration
// ============================================================================

/// @brief Measurement interval (ms) - matches RMS window (~500ms)
#define MEASUREMENT_INTERVAL_MS         500

// ============================================================================
// Display Update Configuration
// ============================================================================

/// @brief OLED display update period (ms) - 5 Hz refresh
#define DISPLAY_UPDATE_PERIOD_MS        200

/// @brief OLED I2C transaction timeout (ms)
#define DISPLAY_I2C_TIMEOUT_MS          100

/// @brief Display initialization timeout (ms)
#define DISPLAY_INIT_TIMEOUT_MS         1000

// ============================================================================
// Button Interface Configuration
// ============================================================================

/// @brief Button debounce time (ms) - hardware + software
#define BUTTON_DEBOUNCE_MS              40

/// @brief Long press detection time (ms)
#define BUTTON_LONG_PRESS_MS            1000

/// @brief Button polling period (ms) - for software debounce
#define BUTTON_POLL_PERIOD_MS           10

// ============================================================================
// MQTT Publishing Configuration
// ============================================================================

/// @brief MQTT publish period (ms) - 5 seconds
#define MQTT_PUBLISH_PERIOD_MS          5000

/// @brief MQTT connection timeout (ms)
#define MQTT_CONNECT_TIMEOUT_MS         30000

/// @brief MQTT keep-alive interval (seconds)
#define MQTT_KEEP_ALIVE_S               60

// ============================================================================
// System Task Configuration
// ============================================================================

/// @brief ADC sensor task stack size (bytes)
#define ADC_TASK_STACK_SIZE             4096

/// @brief ADC sensor task priority (higher for real-time sampling)
#define ADC_TASK_PRIORITY               5

/// @brief UI task stack size (bytes)
#define UI_TASK_STACK_SIZE              3072

/// @brief UI task priority (responsive but not critical)
#define UI_TASK_PRIORITY                3

/// @brief MQTT task stack size (bytes)
#define MQTT_TASK_STACK_SIZE            3072

/// @brief MQTT task priority (background)
#define MQTT_TASK_PRIORITY              2

// ============================================================================
// FreeRTOS Configuration
// ============================================================================

/// @brief Default task delay when idle (ms) - prevents busy waiting
#define TASK_IDLE_DELAY_MS              10

/// @brief Mutex/semaphore timeout (ms) - prevent deadlocks
#define SYNC_TIMEOUT_MS                 1000

/// @brief Queue receive timeout (ms)
#define QUEUE_TIMEOUT_MS                100

// ============================================================================
// Watchdog Configuration
// ============================================================================

/// @brief Watchdog timeout (seconds) - reset if task hangs
#define WATCHDOG_TIMEOUT_S              10

#endif // TIMING_CONFIG_H