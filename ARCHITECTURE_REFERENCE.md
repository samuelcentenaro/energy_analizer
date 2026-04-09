# 🏗️ Layered Architecture Guide - Energy Analyzer

## Overview

This guide documents the **Layered Architecture Pattern** adopted for the Energy Analyzer firmware. All new components and features must follow this pattern to maintain modularity, testability, and scalability.

**Key Document:** `.github/instructions/layered-architecture.instructions.md`

---

## Quick Reference: Which Layer for What?

### 🔴 **DRIVERS Layer** (`components/drivers/`)
**When:** Need direct hardware access
- ADC sampling
- I2C/SPI transactions
- GPIO/UART control
- Interrupt handlers

**Example:** `adc_driver.c` (raw ADC reads, ISR callbacks)

---

### 🟠 **HAL Layer** (`components/hal/`)
**When:** Need hardware abstraction
- Common interface for similar hardware
- Error handling and translation
- Configuration validation
- Resource initialization

**Example:** `adc_hal.h/c` (unified ADC interface, error mapping)

---

### 🟡 **SERVICES Layer** (`components/services/`)
**When:** Building reusable business logic
- RMS calculation
- FFT analysis
- Data filtering
- State machines
- Algorithms

**Example:** `measurement_service.c` (RMS computation, power calculation)

---

### 🟢 **RTOS Layer** (`components/rtos/`)
**When:** Managing tasks and synchronization
- Task creation
- Queue handling
- Semaphore/mutex management
- Event group coordination
- FreeRTOS primitives

**Example:** `sensor_task.c`, `analysis_task.c`

---

### 🔵 **APP Layer** (`components/app/`)
**When:** High-level system behavior
- Application initialization
- Task orchestration
- User interaction
- System state management
- Decision making

**Example:** `energy_analyzer_app.c` (init, task creation, behavior)

---

### ⚪ **UTILS Layer** (`components/utils/`)
**When:** Generic, reusable utilities
- Math functions
- String manipulation
- Data conversion
- Generic algorithms

**Example:** `math_utils.c` (RMS, FFT, filtering helper functions)

---

### ⚫ **NETWORK Layer** (`components/network/`)
**When:** Connectivity and communication
- WiFi management
- MQTT client
- BLE/Bluetooth
- TLS/SSL
- Connection retry logic

**Example:** `mqtt_service.c`, `wifi_manager.c`

---

## Dependency Rules

### ✅ **ALLOWED (Upward)**
```
APP    → RTOS
RTOS   → SERVICES
SERVICES → HAL
HAL    → DRIVERS
UTILS  → (no dependencies)
NETWORK → HAL (for communication hardware)
```

### ❌ **FORBIDDEN (Downward/Sideways)**
```
DRIVERS  → SERVICES (no!)
DRIVERS  → RTOS (no!)
DRIVERS  → APP (no!)
HAL      → RTOS (no!)
SERVICES → RTOS (no!)
```

---

## Implementation Checklist

When creating a new component:

- [ ] **Identify the layer** - What responsibility does it have?
- [ ] **Define inputs/outputs** - What does it need? What does it provide?
- [ ] **Create the API** - Header file with clear interface
- [ ] **Implement with error handling** - All paths covered with `esp_err_t`
- [ ] **Add documentation** - Doxygen comments
- [ ] **Create CMakeLists.txt** - With correct REQUIRES
- [ ] **Test in isolation** - Before integration
- [ ] **Integrate upward only** - Call from the right layer
- [ ] **Add logging** - ERROR/WARN/INFO/DEBUG levels
- [ ] **Review for memory leaks** - malloc/free pairing, static preferred

---

## FreeRTOS Patterns

### Task Template
```c
static void my_task(void *arg)
{
    ESP_LOGI(TAG, "Task started");
    
    while (1) {
        // Wait for event/data
        if (xQueueReceive(g_queue, &data, pdMS_TO_TICKS(1000))) {
            // Process
            service_process(&data);
            
            // Send result
            xQueueSend(g_result_queue, &result, 0);
        }
    }
}

// Create in APP layer
xTaskCreate(my_task, "my_task", STACK_SIZE, NULL, PRIORITY, NULL);
```

### ISR Pattern
```c
// Minimal - only queue data
static void isr_handler(void *arg)
{
    uint16_t value = READ_HARDWARE();
    
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(g_queue, &value, &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Heavy processing goes in task, not ISR
```

---

## Memory Management

### Preferred: Static Allocation
```c
#define BUFFER_SIZE 256
static uint8_t g_buffer[BUFFER_SIZE];      // Stack memory, safe
static adc_data_t g_measurements[100];     // Compile-time size
```

### If Dynamic Needed
```c
void *mem = malloc(size);
if (mem == NULL) {
    ESP_LOGE(TAG, "Malloc failed");
    return ESP_ERR_NO_MEM;
}

// Use...

free(mem);
mem = NULL;  // Prevent use-after-free
```

**Never:**
```c
// ❌ malloc in ISR
// ❌ Unbounded allocations
// ❌ malloc without free
// ❌ malloc in critical sections
```

---

## Error Handling Template

```c
esp_err_t my_function(const config_t *cfg)
{
    // 1. Validate input
    if (cfg == NULL) {
        ESP_LOGE(TAG, "Null config");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 2. Check state
    if (!g_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 3. Acquire resources
    if (!xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000))) {
        ESP_LOGW(TAG, "Resource busy");
        return ESP_ERR_TIMEOUT;
    }
    
    // 4. Perform operation with error checks
    esp_err_t ret = lower_layer_function();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Lower layer failed: 0x%02x", ret);
        xSemaphoreGive(g_mutex);
        return ret;
    }
    
    // 5. Release resources
    xSemaphoreGive(g_mutex);
    
    // 6. Log success
    ESP_LOGD(TAG, "Operation complete");
    
    return ESP_OK;
}
```

---

## Logging Levels

| Level | Use When | Example |
|-------|----------|---------|
| **ERROR** | Cannot continue, critical failure | `ESP_LOGE(TAG, "ADC initialization failed")` |
| **WARN** | Unexpected but recoverable, degraded mode | `ESP_LOGW(TAG, "Sensor out of range")` |
| **INFO** | Important system event, milestone reached | `ESP_LOGI(TAG, "WiFi connected")` |
| **DEBUG** | Development information, values, states | `ESP_LOGD(TAG, "ADC raw=0x%04x", raw)` |

---

## Build Integration

### New Component CMakeLists.txt
```cmake
idf_component_register(
    SRCS "my_component.c"
    INCLUDE_DIRS "include"
    REQUIRES other_components
    PRIV_REQUIRES freertos esp_log
)
```

### Component REQUIRES
- List only what you **use** in header files (public interface)
- `freertos`, `esp_log` go in PRIV_REQUIRES (private, not exposed)
- Never create circular dependencies

---

## Testing Strategy

### Unit Test Example
```c
// test/test_rms_calculation.c
#include "unity.h"
#include "measurement_service.h"

TEST_CASE("RMS calculation - sine wave", "[services]")
{
    float samples[] = {0, 1, 0, -1};  // Sine approximation
    float rms = math_rms(samples, 4);
    
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.707f, rms);
}
```

### Integration Test Example
```c
TEST_CASE("ADC to MQTT pipeline", "[integration]")
{
    // 1. Initialize all layers
    hal_adc_init(&config);
    
    // 2. Create queues
    QueueHandle_t q = xQueueCreate(10, sizeof(data_t));
    
    // 3. Launch tasks
    xTaskCreate(sensor_task, "sensor", 4096, NULL, 5, NULL);
    xTaskCreate(mqtt_task, "mqtt", 2048, NULL, 2, NULL);
    
    // 4. Wait and verify
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // 5. Check results
    TEST_ASSERT(mqtt_messages_received > 0);
}
```

---

## Performance Considerations

### Task Priorities
```
Priority 5: Sensor/ADC (real-time, critical timing)
Priority 4: Analysis (high, CPU-intensive)
Priority 3: UI/Display (medium, user interaction)
Priority 2: Communication/MQTT (low, background)
Priority 1: App/System (lowest, orchestration)
```

### Stack Sizes
```
ADC Task:     4096 bytes  (complex operations)
Analysis:     3072 bytes  (FFT, math)
UI:           2048 bytes  (simple)
MQTT:         2048 bytes  (simple)
App:          2048 bytes  (minimal)
```

### Queue Sizes
```
ADC samples:  256 items  (high frequency)
Results:      10 items   (low frequency)
MQTT:         5 items    (low frequency)
```

---

## Debugging Commands

```bash
# Monitor logs and system state
idf.py monitor

# Build with verbose output
idf.py -v build

# Reset to clean state
idf.py fullclean

# Monitor specific log level
idf.py monitor --print-filter="ADC_SENSOR:D"  # Debug level for ADC_SENSOR tag

# Get build size
idf.py size

# Analyze partition table
idf.py partition-table
```

---

## Common Mistakes to Avoid

| Mistake | Impact | Solution |
|---------|--------|----------|
| Business logic in driver | Hard to test, cannot reuse | Move to SERVICE layer |
| FreeRTOS calls in service | Tight coupling, poor modularity | Move to RTOS layer |
| Circular dependencies | Will not compile, unmaintainable | Follow dependency rules |
| Heavy ISR processing | Missed deadlines, sample loss | Queue data only, process in task |
| Dynamic malloc everywhere | Memory fragmentation, leaks | Use static allocation |
| Missing error checks | Silent failures, hard debugging | Check all return values |
| Loose logging | Too much spam or too quiet | Use levels appropriately |

---

## Phase 2+ Component Examples

### Display Component
```
components/display/
├── drivers/
│   └── ssd1306_driver.c     // I2C writes, ISR
├── hal/
│   └── display_hal.c         // Abstraction
├── services/
│   └── ui_service.c          // Menu logic
└── rtos/
    └── display_task.c        // Update task
```

### MQTT Component
```
components/network/
├── hal/
│   └── mqtt_hal.c            // Client abstraction
├── services/
│   └── mqtt_service.c        // Publish formatting
└── rtos/
    └── mqtt_task.c           // Connection management
```

---

## References

- **Main Architecture:** `.github/instructions/layered-architecture.instructions.md`
- **ESP32 Specialist:** `.github/instructions/esp32-specialist.instructions.md`
- **Coding Standards:** `CODING_STANDARDS.md`
- **Hardware:** `HARDWARE_DIAGRAM.md`, `HW_CONNECTIONS.txt`

---

**Follow this pattern consistently for Phase 2, Phase 3, and beyond!** 🚀

Last Updated: April 6, 2026
