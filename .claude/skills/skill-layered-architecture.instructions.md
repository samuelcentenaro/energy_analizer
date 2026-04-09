---
name: layered-architecture
description: "Layered architecture pattern for ESP32 Energy Analyzer. Use when: designing component structure, organizing firmware modules, planning communication patterns, implementing services. Ensures: clean separation of concerns, modular design, scalability, maintainability."
applyTo: "components/**/*.{c,h}"
---

# 🏗️ Layered Architecture Pattern — Energy Analyzer Firmware

## 📐 Architecture Overview

The Energy Analyzer firmware follows a **strict layered architecture** to ensure modularity, testability, and maintainability. Each layer has a single responsibility and communicates only with adjacent layers.

```
┌─────────────────────────────────────────┐
│           APP LAYER (app/)              │  High-level behavior
│   - System logic, decision making       │
├─────────────────────────────────────────┤
│        SERVICES LAYER (services/)       │  Business logic
│   - Orchestration, algorithms, state    │
├─────────────────────────────────────────┤
│        RTOS LAYER (rtos/)               │  Task management
│   - Tasks, queues, event groups         │
├─────────────────────────────────────────┤
│        HAL LAYER (hal/)                 │  Hardware abstraction
│   - Unified interface, error handling   │
├─────────────────────────────────────────┤
│       DRIVERS LAYER (drivers/)          │  Direct hardware access
│   - Register access, ISR callbacks      │
└─────────────────────────────────────────┘

Side layers:
├─ utils/    → Generic utilities (math, string, conversion)
├─ network/  → Connectivity (WiFi, MQTT, BLE)
└─ config/   → Configuration (hardware_config.h, etc.)
```

---

## 🎯 Layer Responsibilities

### **DRIVERS Layer** (`components/drivers/`)

**Purpose:** Direct hardware access with minimal abstraction

**Responsibilities:**
- Register-level I/O
- ISR callbacks (minimal processing)
- Low-level error handling
- DMA configuration
- Interrupt management

**Characteristics:**
- ✅ Hardware-specific code only
- ✅ Minimal processing in ISR
- ✅ Stateless if possible
- ✅ No business logic
- ❌ No FreeRTOS calls (except xHigherPriorityTaskWoken)
- ❌ No system-wide decisions

**Example:**
```c
// drivers/adc_driver.c
static void adc_isr_callback(void *arg)
{
    // CRITICAL: Must complete in <100µs
    // Extract raw data only
    uint16_t sample = ADC_READ_REG();
    
    // Send to queue if available
    if (xQueueSendFromISR(g_adc_queue, &sample, NULL)) {
        // Success
    }
}
```

---

### **HAL Layer** (`components/hal/`)

**Purpose:** Abstract hardware differences, provide unified interface

**Responsibilities:**
- Hardware abstraction
- Error translation
- Resource initialization
- Configuration validation
- Platform-specific adaptations

**Characteristics:**
- ✅ Single interface for different peripherals
- ✅ Error checking and validation
- ✅ Resource management
- ✅ Timeout handling
- ❌ No business logic
- ❌ No task creation

**Example:**
```c
// hal/adc_hal.h
esp_err_t adc_hal_init(const adc_hal_config_t *config);
esp_err_t adc_hal_read(uint16_t *value);
esp_err_t adc_hal_deinit(void);

// hal/adc_hal.c - Implementation
esp_err_t adc_hal_read(uint16_t *value)
{
    if (value == NULL) return ESP_ERR_INVALID_ARG;
    if (!g_adc_initialized) return ESP_ERR_INVALID_STATE;
    
    // Call driver, translate errors
    esp_err_t ret = driver_adc_read(value);
    if (ret != 0) {
        return ESP_ERR_HW_FAILURE;
    }
    
    return ESP_OK;
}
```

---

### **SERVICES Layer** (`components/services/`)

**Purpose:** Reusable logic, algorithms, orchestration

**Responsibilities:**
- Business logic implementation
- Data processing
- Algorithm execution (RMS, FFT, etc.)
- Resource coordination
- State machines
- Error recovery

**Characteristics:**
- ✅ Calls HAL functions only
- ✅ No direct hardware access
- ✅ Stateful operations
- ✅ Orchestration logic
- ❌ No task creation
- ❌ No FreeRTOS calls (queues, semaphores)

**Example:**
```c
// services/measurement_service.c
typedef struct {
    float voltage_rms;
    float current_rms;
    float power_real;
    uint32_t samples_count;
} measurement_result_t;

esp_err_t measurement_calculate_rms(
    const uint16_t *samples,
    uint32_t count,
    measurement_result_t *result)
{
    if (!samples || !result) return ESP_ERR_INVALID_ARG;
    if (count == 0) return ESP_ERR_INVALID_ARG;
    
    // Pure computation, no hardware calls
    result->voltage_rms = compute_rms(samples, count);
    result->samples_count = count;
    
    return ESP_OK;
}
```

---

### **RTOS Layer** (`components/rtos/`)

**Purpose:** Task organization and synchronization

**Responsibilities:**
- Task creation and management
- Queue handling
- Event group management
- Semaphore/mutex protection
- Timing and delays
- Task priorities

**Characteristics:**
- ✅ FreeRTOS API calls
- ✅ Inter-task communication
- ✅ Resource synchronization
- ✅ Task state management
- ❌ No HAL/service calls (tasks call up, not down)
- ❌ No hardware-specific code

**Example:**
```c
// rtos/sensor_task.c
static void sensor_task(void *arg)
{
    measurement_result_t result;
    
    while (1) {
        // Wait for ADC data
        adc_sample_t sample;
        if (xQueueReceive(g_adc_queue, &sample, 
                         pdMS_TO_TICKS(1000))) {
            // Process via service
            measurement_calculate_rms(&sample, 1, &result);
            
            // Send to app via queue
            xQueueSend(g_result_queue, &result, 0);
        }
    }
}
```

---

### **APP Layer** (`components/app/`)

**Purpose:** High-level behavior and system rules

**Responsibilities:**
- Application logic
- System orchestration
- User interaction
- State machines
- Decision making
- Coordination of tasks

**Characteristics:**
- ✅ Calls RTOS functions
- ✅ System-level logic
- ✅ Event handling
- ✅ Behavior definition
- ❌ No direct hardware access
- ❌ No computation-heavy operations

**Example:**
```c
// app/energy_analyzer_app.c
void app_init(void)
{
    // Initialize components bottom-up
    hal_adc_init();
    
    // Create tasks
    xTaskCreate(sensor_task, "sensor", 4096, NULL, 5, NULL);
    xTaskCreate(control_task, "control", 2048, NULL, 3, NULL);
    
    // System ready
    ESP_LOGI(TAG, "System initialized");
}
```

---

### **UTILS Layer** (`components/utils/`)

**Purpose:** Generic reusable functions

**Responsibilities:**
- Math utilities
- String handling
- Data conversion
- Generic algorithms
- Helper functions

**Characteristics:**
- ✅ No dependencies on other layers
- ✅ Platform-agnostic
- ✅ Reusable across projects
- ❌ No ESP32/FreeRTOS specific code
- ❌ No business logic

**Example:**
```c
// utils/math_utils.c
float math_rms(const float *values, uint32_t count)
{
    if (!values || count == 0) return 0.0f;
    
    double sum_sq = 0.0;
    for (uint32_t i = 0; i < count; i++) {
        sum_sq += values[i] * values[i];
    }
    
    return sqrtf(sum_sq / count);
}
```

---

### **NETWORK Layer** (`components/network/`)

**Purpose:** Connectivity stack

**Responsibilities:**
- WiFi management
- MQTT client
- BLE GATT
- TLS/SSL communication
- Network error handling
- Connection retry logic

**Characteristics:**
- ✅ Modular per protocol
- ✅ Error recovery
- ✅ State machines
- ✅ Event callbacks
- ❌ No hardware-specific code
- ❌ No computation logic

**Example:**
```c
// network/mqtt_service.c
esp_err_t mqtt_publish(const char *topic, const char *data)
{
    if (!topic || !data) return ESP_ERR_INVALID_ARG;
    if (g_mqtt_state != MQTT_CONNECTED) 
        return ESP_ERR_INVALID_STATE;
    
    esp_mqtt_client_publish(g_mqtt_client, topic, data, 0, 1);
    return ESP_OK;
}
```

---

## 🔁 FreeRTOS Design Pattern

### Task Organization

```c
// One task per major responsibility

// sensor_task: Acquires raw data
static void sensor_task(void *arg)
{
    xQueueReceive(g_data_queue, &data, portMAX_DELAY);
    // Send to next stage
}

// analysis_task: Processes data
static void analysis_task(void *arg)
{
    xQueueReceive(g_data_queue, &data, portMAX_DELAY);
    // Perform calculations
    xQueueSend(g_result_queue, &result, 0);
}

// communication_task: Sends data
static void communication_task(void *arg)
{
    xQueueReceive(g_result_queue, &result, portMAX_DELAY);
    // Publish to MQTT
}
```

### Synchronization Primitives

**Queues** → Data exchange between tasks
```c
QueueHandle_t g_data_queue = xQueueCreate(10, sizeof(data_t));
xQueueSend(g_data_queue, &data, portMAX_DELAY);
xQueueReceive(g_data_queue, &data, pdMS_TO_TICKS(1000));
```

**Event Groups** → System state management
```c
EventGroupHandle_t g_system_events = xEventGroupCreate();
xEventGroupSetBits(g_system_events, WIFI_CONNECTED);
EventBits_t bits = xEventGroupWaitBits(g_system_events, 
    WIFI_CONNECTED, pdFALSE, pdTRUE, pdMS_TO_TICKS(30000));
```

**Semaphores** → Resource protection
```c
SemaphoreHandle_t g_mutex = xSemaphoreCreateMutex();
xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000));
// Critical section
xSemaphoreGive(g_mutex);
```

### Task Creation Pattern

```c
void system_init(void)
{
    // Create all tasks with appropriate priorities
    // Higher number = higher priority
    
    xTaskCreate(sensor_task, "sensor", 4096, NULL, 5, NULL);     // Real-time
    xTaskCreate(analysis_task, "analysis", 3072, NULL, 4, NULL); // High
    xTaskCreate(comm_task, "comm", 2048, NULL, 2, NULL);         // Low
    xTaskCreate(app_task, "app", 2048, NULL, 1, NULL);           // Lowest
}
```

---

## 🔧 Best Practices

### Memory Management

**✅ Prefer static allocation:**
```c
static uint32_t g_buffer[BUFFER_SIZE];  // Compile-time
static adc_data_t g_measurements[100];  // Known size
```

**❌ Avoid dynamic allocation:**
```c
uint8_t *buffer = malloc(size);  // Fragmentation risk
```

**If dynamic allocation needed:**
```c
void *mem = malloc(size);
if (mem == NULL) {
    ESP_LOGE(TAG, "Memory allocation failed");
    return ESP_ERR_NO_MEM;
}
// Use memory
free(mem);
mem = NULL;  // Prevent use-after-free
```

### Error Handling

**Always check return values:**
```c
esp_err_t ret = hal_adc_init(&config);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "ADC init failed: 0x%02x", ret);
    return ret;  // Propagate error
}
```

**Use esp_err_t consistently:**
```c
esp_err_t my_function(void)
{
    esp_err_t ret = ESP_OK;
    
    ret = other_function();
    if (ret != ESP_OK) {
        return ret;  // Early exit
    }
    
    // Continue only if success
    return ESP_OK;
}
```

### Logging

**Use appropriate levels:**
```c
ESP_LOGE(TAG, "Fatal: ADC init failed");      // Cannot continue
ESP_LOGW(TAG, "Warning: Low battery");        // Degraded mode
ESP_LOGI(TAG, "System started successfully"); // Important event
ESP_LOGD(TAG, "ADC value: %d", raw);          // Development only
```

---

## 📡 Communication Patterns

### ISR to Task Communication

**Bad (ISR with processing):**
```c
static void adc_isr_bad(void *arg)
{
    uint16_t sample = READ_ADC();
    // WRONG: Complex processing in ISR!
    float rms = calculate_rms(sample);
    update_display(rms);
}
```

**Good (ISR queues data):**
```c
static void adc_isr_good(void *arg)
{
    uint16_t sample = READ_ADC();
    // CORRECT: Just queue, task processes
    BaseType_t higher_prio = pdFALSE;
    xQueueSendFromISR(g_adc_queue, &sample, &higher_prio);
    portYIELD_FROM_ISR(higher_prio);
}

static void processing_task(void *arg)
{
    uint16_t sample;
    while (xQueueReceive(g_adc_queue, &sample, portMAX_DELAY)) {
        float rms = calculate_rms(sample);  // Safe in task
        // Send result to app layer
    }
}
```

### Inter-Task Communication

**Queue for data:**
```c
// Sensor → Analysis
adc_sample_t sample;
xQueueReceive(g_data_queue, &sample, pdMS_TO_TICKS(100));
```

**Event Group for state:**
```c
// Notify multiple tasks of WiFi status
if (connected) {
    xEventGroupSetBits(g_events, WIFI_CONNECTED);
} else {
    xEventGroupClearBits(g_events, WIFI_CONNECTED);
}
```

---

## 🔍 Debugging Strategies

### Monitor Performance

```bash
$ idf.py monitor

# Look for:
# - Heap usage (free: XXXXX bytes)
# - Stack watermark (stack watermark: XXXXX bytes)
# - Task states
# - CPU load
```

### Detect Memory Leaks

```c
// Enable in sdkconfig
CONFIG_HEAP_TRACING_DEST_INTERNAL=y
CONFIG_HEAP_TRACING_DEST_UART=y

// In code
heap_trace_start(HEAP_TRACE_LEAKS);
// ... run code that might leak ...
heap_trace_stop();
heap_trace_dump(stdout);
```

### Profile Task Timing

```c
static uint32_t g_task_start_time;

static void my_task(void *arg)
{
    while (1) {
        g_task_start_time = esp_timer_get_time();
        
        // Do work
        work();
        
        uint32_t elapsed = esp_timer_get_time() - g_task_start_time;
        ESP_LOGD(TAG, "Task took %lld µs", elapsed);
    }
}
```

---

## 🌐 Connectivity Patterns

### WiFi with Auto-Reconnect

```c
// network/wifi_service.c
typedef struct {
    char ssid[32];
    char password[64];
    uint8_t retry_count;
} wifi_config_t;

esp_err_t wifi_connect(const wifi_config_t *cfg)
{
    // Initialize, set callbacks
    // Return when connected
    xEventGroupWaitBits(g_wifi_events, WIFI_CONNECTED, 
                       pdFALSE, pdTRUE, pdMS_TO_TICKS(30000));
    return ESP_OK;
}

// WiFi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_STA_CONNECTED) {
        xEventGroupSetBits(g_wifi_events, WIFI_CONNECTED);
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(g_wifi_events, WIFI_CONNECTED);
        // Auto-reconnect handled by ESP-IDF
    }
}
```

### MQTT with Queuing

```c
// network/mqtt_service.c
esp_err_t mqtt_publish_async(const char *topic, const char *data)
{
    // Queue message if not connected
    mqtt_message_t msg = {topic, data};
    xQueueSend(g_mqtt_queue, &msg, pdMS_TO_TICKS(100));
    return ESP_OK;
}

static void mqtt_task(void *arg)
{
    mqtt_message_t msg;
    while (xQueueReceive(g_mqtt_queue, &msg, portMAX_DELAY)) {
        if (mqtt_is_connected()) {
            esp_mqtt_client_publish(g_mqtt_client, msg.topic, 
                                   msg.data, 0, 1);
        }
    }
}
```

---

## ⚡ Response Style for This Architecture

### When Implementing a New Component:

1. **Identify the layer** → Which responsibility does it have?
2. **Define the interface** → What does it expose upward?
3. **Implement the layer** → Handle its specific concern
4. **Test in isolation** → Mock dependencies if needed
5. **Integrate upward** → Call only from the right layers

### Example Request:
> "Create a temperature sensor service"

**Expected Response:**
- Driver layer (I2C, read raw value)
- HAL layer (abstraction, error handling)
- Service layer (averaging, smoothing)
- RTOS layer (task if needed)
- Complete code with error handling

---

## 📌 Quick Reference

| Layer | Calls | Called By | Examples |
|-------|-------|-----------|----------|
| **APP** | RTOS | Integration | State machines, behavior |
| **RTOS** | Services | App | Tasks, queues, events |
| **SERVICES** | HAL | RTOS | Algorithms, processing |
| **HAL** | Drivers | Services | Error translation |
| **DRIVERS** | Hardware | HAL | Registers, ISR |

---

## ✅ Checklist Before Implementation

- [ ] Identify target layer
- [ ] Define dependencies (what it needs)
- [ ] Define interface (what it provides)
- [ ] Plan error handling
- [ ] Plan resource cleanup
- [ ] Document assumptions
- [ ] Test each layer independently

---

**This architecture ensures:**
- ✅ Modularity and reusability
- ✅ Easy testing and debugging
- ✅ Clear dependencies
- ✅ Scalability
- ✅ Maintainability
- ✅ Team collaboration

---

**Let's build modular, scalable firmware! 🚀**