/**
 * @file math_utils.c
 * @brief Mathematical utility functions implementation
 */

#include "math_utils.h"
#include <math.h>

/**
 * @brief Calculate Root Mean Square (RMS) of an array of float values
 */
float math_rms(const float *values, uint32_t count)
{
    if (values == NULL || count == 0) {
        return 0.0f;
    }

    double sum_squares = 0.0;

    for (uint32_t i = 0; i < count; i++) {
        sum_squares += (double)values[i] * (double)values[i];
    }

    return sqrtf(sum_squares / (double)count);
}

/**
 * @brief Calculate mean (average) of an array of float values
 */
float math_mean(const float *values, uint32_t count)
{
    if (values == NULL || count == 0) {
        return 0.0f;
    }

    double sum = 0.0;

    for (uint32_t i = 0; i < count; i++) {
        sum += (double)values[i];
    }

    return (float)(sum / (double)count);
}

/**
 * @brief Calculate standard deviation of an array of float values
 */
float math_stddev(const float *values, uint32_t count)
{
    if (values == NULL || count < 2) {
        return 0.0f;
    }

    float mean = math_mean(values, count);
    double sum_squares = 0.0;

    for (uint32_t i = 0; i < count; i++) {
        float diff = values[i] - mean;
        sum_squares += (double)diff * (double)diff;
    }

    return sqrtf(sum_squares / (double)(count - 1));
}

/**
 * @brief Apply simple moving average filter
 */
float math_moving_average(float input, float *buffer, uint32_t buffer_size, uint32_t *buffer_index)
{
    if (buffer == NULL || buffer_size == 0 || buffer_index == NULL) {
        return input;
    }

    // Store new value in buffer
    buffer[*buffer_index] = input;
    *buffer_index = (*buffer_index + 1) % buffer_size;

    // Calculate average
    return math_mean(buffer, buffer_size);
}

/**
 * @brief Constrain a value within a range
 */
float math_constrain(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
 * @brief Map a value from one range to another
 */
float math_map(float value, float in_min, float in_max, float out_min, float out_max)
{
    if (in_max == in_min) return out_min;

    float normalized = (value - in_min) / (in_max - in_min);
    return out_min + normalized * (out_max - out_min);
}