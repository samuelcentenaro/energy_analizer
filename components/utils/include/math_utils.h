/**
 * @file math_utils.h
 * @brief Mathematical utility functions
 *
 * This module provides common mathematical operations used throughout
 * the Energy Analyzer, including RMS calculation, filtering, and statistics.
 */

#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Calculate Root Mean Square (RMS) of an array of float values
 *
 * @param values Pointer to array of float values
 * @param count Number of values in the array
 * @return RMS value, or 0.0f if invalid parameters
 */
float math_rms(const float *values, uint32_t count);

/**
 * @brief Calculate mean (average) of an array of float values
 *
 * @param values Pointer to array of float values
 * @param count Number of values in the array
 * @return Mean value, or 0.0f if invalid parameters
 */
float math_mean(const float *values, uint32_t count);

/**
 * @brief Calculate standard deviation of an array of float values
 *
 * @param values Pointer to array of float values
 * @param count Number of values in the array
 * @return Standard deviation, or 0.0f if invalid parameters
 */
float math_stddev(const float *values, uint32_t count);

/**
 * @brief Apply simple moving average filter
 *
 * @param input New input value
 * @param buffer Circular buffer for previous values
 * @param buffer_size Size of the buffer
 * @param buffer_index Current index in buffer (will be updated)
 * @return Filtered output value
 */
float math_moving_average(float input, float *buffer, uint32_t buffer_size, uint32_t *buffer_index);

/**
 * @brief Constrain a value within a range
 *
 * @param value Input value
 * @param min Minimum allowed value
 * @param max Maximum allowed value
 * @return Constrained value
 */
float math_constrain(float value, float min, float max);

/**
 * @brief Map a value from one range to another
 *
 * @param value Input value
 * @param in_min Input range minimum
 * @param in_max Input range maximum
 * @param out_min Output range minimum
 * @param out_max Output range maximum
 * @return Mapped value
 */
float math_map(float value, float in_min, float in_max, float out_min, float out_max);

#endif // MATH_UTILS_H