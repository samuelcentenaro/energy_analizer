#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

/**
 * @file hardware_config.h
 * @brief Hardware configuration constants for Energy Analyzer
 * 
 * This file contains all hardware-specific definitions including
 * pin assignments, sensor specifications, and physical constants.
 * 
 * @note All pin definitions follow ESP32 GPIO numbering
 * @note Modify these values only if hardware changes
 */

// ============================================================================
// Legacy Internal ADC Configuration (Unused in active firmware)
// ============================================================================

/*
 * The active acquisition path uses the external ADS1015 over I2C.
 * These internal ADC definitions are kept only so legacy components still
 * compile, but they do not represent the active hardware routing.
 *
 * IMPORTANT:
 * - GPIO34 and GPIO35 are currently assigned to user buttons.
 * - Do not reuse these legacy ADC definitions in the active firmware path.
 * - Remove the legacy internal ADC path entirely in a future cleanup phase.
 */

/// @brief Legacy ADC Unit for deprecated internal-ADC path
#define ADC_UNIT                ADC_UNIT_1

/// @brief Legacy voltage channel (deprecated internal ADC path only)
#define ADC_CHANNEL_VOLTAGE     ADC_CHANNEL_7  // Legacy mapping only

/// @brief Legacy current channel (deprecated internal ADC path only)
#define ADC_CHANNEL_CURRENT     ADC_CHANNEL_6  // Legacy mapping only

/// @brief Legacy attenuation for deprecated internal-ADC path
#define ADC_ATTEN               ADC_ATTEN_DB_12

/// @brief Legacy resolution for deprecated internal-ADC path
#define ADC_BITWIDTH            ADC_BITWIDTH_DEFAULT

// ============================================================================
// ADS1015 ADC Configuration (External ADC)
// ============================================================================

/// @brief ADS1015 I2C address (default 0x48)
#define ADS1015_I2C_ADDRESS     0x48

/// @brief ADS1015 voltage channel configuration (differential A0-A1, active path)
#define ADS1015_CHANNEL_VOLTAGE 0  // A0-A1 differential

/// @brief ADS1015 current channel configuration (differential A2-A3, active path)
#define ADS1015_CHANNEL_CURRENT 1  // A2-A3 differential

/// @brief ADS1015 PGA gain for voltage channel (2 = ±2.048V)
#define ADS1015_GAIN_VOLTAGE    2

/// @brief ADS1015 PGA gain for current channel (1 = ±4.096V)
#define ADS1015_GAIN_CURRENT    1

/// @brief ADS1015 data rate (1600 SPS)
#define ADS1015_DATA_RATE       4  // 1600 SPS

// ============================================================================
// I2C Configuration (Shared for ADS1015 and OLED)
// ============================================================================

/// @brief I2C Port number
#define CONFIG_I2C_PORT         I2C_NUM_0

/// @brief I2C SDA pin (GPIO21)
#define CONFIG_I2C_SDA_PIN      GPIO_NUM_21

/// @brief I2C SCL pin (GPIO22)
#define CONFIG_I2C_SCL_PIN      GPIO_NUM_22

/// @brief I2C Clock frequency (400 kHz)
#define CONFIG_I2C_FREQUENCY_HZ 400000

/// @brief SSD1306 OLED I2C address (default 0x3C)
#define OLED_I2C_ADDRESS        0x3C

// ============================================================================
// GPIO Configuration (Buttons)
// ============================================================================

/// @brief Button UP pin (GPIO35, active hardware mapping)
#define BUTTON_PIN_UP           GPIO_NUM_16

/// @brief Button DOWN pin (GPIO34, active hardware mapping)
#define BUTTON_PIN_DOWN         GPIO_NUM_17

/// @brief Button SELECT pin (GPIO32, active hardware mapping)
#define BUTTON_PIN_SELECT       GPIO_NUM_15

// ============================================================================
// GPIO Configuration (Status LED)
// ============================================================================

/// @brief Status LED pin (built-in blue LED on ESP32)
#define STATUS_LED_PIN          GPIO_NUM_2

// ============================================================================
// UART Configuration (Debug Serial)
// ============================================================================

/// @brief Debug UART port
#define DEBUG_UART_NUM          UART_NUM_0

/// @brief Debug UART baud rate
#define DEBUG_UART_BAUD_RATE    115200

// ============================================================================
// Sensor Electrical Specifications
// ============================================================================

/// @brief ZMPT101B input voltage range (RMS)
#define VOLTAGE_SENSOR_RANGE_V  250.0f

/// @brief SCT013-030 input current range (RMS)
#define CURRENT_SENSOR_RANGE_A  30.0f

/// @brief Burden resistor value for SCT013 (ohms)
#define CURRENT_BURDEN_RESISTOR 18.0f

/// @brief Voltage divider ratio for ZMPT101B (if used)
#define VOLTAGE_DIVIDER_RATIO   1.0f  // 1:1 if no divider

// ============================================================================
// Power Supply Specifications
// ============================================================================

/// @brief Main supply voltage (V)
#define SUPPLY_VOLTAGE_V        3.3f

/// @brief Maximum current draw (A) - for power budgeting
#define MAX_CURRENT_DRAW_A      0.2f

#endif // HARDWARE_CONFIG_H
