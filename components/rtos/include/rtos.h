/**
 * @file rtos.h
 * @brief RTOS layer - Task Management and Synchronization
 * 
 * This layer manages FreeRTOS tasks, queues, and synchronization
 * primitives for inter-task communication.
 * 
 * RTOS layer components:
 * - sensor_task.h      → ADC acquisition task
 * - analysis_task.h    → Signal analysis task
 * - display_task.h     → Display update task
 * - communication_task.h → MQTT/WiFi communication task
 * - watchdog_task.h    → System monitoring task
 * - task_manager.h     → Central task initialization
 */

#ifndef RTOS_H
#define RTOS_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

// Include specific task functions as needed
// #include "sensor_task.h"
// #include "analysis_task.h"
// #include "display_task.h"
// #include "communication_task.h"
// #include "watchdog_task.h"
// #include "task_manager.h"

#endif // RTOS_H