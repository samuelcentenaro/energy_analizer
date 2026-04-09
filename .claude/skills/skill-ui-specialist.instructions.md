---
name: ui-specialist
description: "Implement and maintain the Energy Analyzer user interface (OLED display + 3 buttons). Use when: designing screens, implementing button handling, creating UI layouts, managing screen navigation. Ensures: consistent UX, proper debouncing, clean separation from business logic."
applyTo: "components/ui/**/*.{c,h}"
---

# 🎨 UI Specialist — Energy Analyzer OLED Interface

## 👤 Role Definition

You are a **UI/UX embedded engineer** specialized in designing and implementing user interfaces for embedded systems with limited resources. Your focus is creating **intuitive, responsive, and maintainable UI** on single-line character OLED displays with button-based navigation.

---

## 📱 Display Context

### Hardware Specifications
- **Display Type:** SSD1306, monochrome, 128×64 pixels
- **Interface:** I2C, address 0x3C
- **I2C Port:** I2C_NUM_0, GPIO21 (SDA), GPIO22 (SCL), 400 kHz
- **Resolution:** 8 pages (rows of 8 pixels each), 128 columns (bits)
- **Font:** Custom 5×7 pixel glyphs (already implemented)

### Current Implementation Location
```
components/app/energy_analyzer_app.c
├── oled_init()              → SSD1306 init sequence
├── oled_write_command()     → I2C command transfer
├── oled_write_data()        → I2C data transfer
├── oled_clear()             → Clear all 8 pages
├── oled_set_cursor()        → Set page/column
├── oled_draw_text()         → Draw ASCII via 5×7 glyphs
└── oled_get_glyph()         → Glyph lookup table
```

**Problem:** OLED code is embedded in `energy_analyzer_app.c`. Extraction to `components/ui/` is pending.

---

## 🔘 Button Hardware

### Pinout
| Button | GPIO | Active Level |
|--------|------|--------------|
| UP     | 35   | LOW (pressed = 0) |
| DOWN   | 34   | LOW |
| SELECT | 32   | LOW |

### Current Implementation
```c
// components/board_hal/board_hal.c
esp_err_t board_button_read(int gpio_num, bool *pressed);
// Uses: Raw GPIO read (GPIO.in / GPIO.in1.val), no driver
```

**Status:** Raw read only, no debounce, no events, no ISR.

---

## 🎯 Required UI Components

### Structure Plan (NEW `components/ui/`)
```
components/ui/
├── include/
│   ├── ui_public.h          # Main entry points: ui_init(), ui_render(), ui_process_buttons()
│   ├── ui_types.h           # enums, structs, constants
│   ├── ui_button.h          # Button events and handler API
│   ├── ui_screen.h          # Screen registry and callbacks
│   └── ui_widget.h          # Widget API (progress bar, chart, icon)
├── src/
│   ├── ui.c                 # Context, init/deinit, event loop
│   ├── ui_button_handler.c  # Debounced button driver (poll-based)
│   ├── ui_screen_manager.c  # Active screen routing, transitions
│   ├── ui_renderer.c        # OLED abstraction (wrapper over i2c_hal)
│   └── widgets/
│       ├── widget_progress_bar.c
│       ├── widget_chart.c
│       └── widget_icon.c
├── screens/
│   ├── screen_base.c        # Base screen struct + helpers
│   ├── screen_summary.c     # Main measurement screen (RMS: V, I, P, PF)
│   ├── screen_raw.c         # Raw ADC values + calibration coeffs
│   ├── screen_diagnostic.c  # Status checklist (I2C, ADS, WiFi, MQTT)
│   ├── screen_wifi.c        # WiFi status, SSID, IP, RSSI
│   ├── screen_mqtt.c        # MQTT status, broker, publish count
│   └── screen_calibration.c # Calibration view + apply routines
└── CMakeLists.txt
```

---

## 🔄 Navigation Architecture

### State Machine
```c
typedef enum {
    UI_SCREEN_SUMMARY = 0,
    UI_SCREEN_RAW,
    UI_SCREEN_DIAGNOSTIC,
    UI_SCREEN_WIFI,
    UI_SCREEN_MQTT,
    UI_SCREEN_CALIBRATION,
    UI_SCREEN_MENU,           // Optional: master menu
    UI_SCREEN_COUNT
} ui_screen_id_t;

typedef enum {
    UI_NAV_MODE_AUTO_ROTATE = 0,   // Cycle through screens after timeout
    UI_NAV_MODE_MANUAL            // Only change via buttons
} ui_nav_mode_t;
```

### Screen Lifecycle
```c
// Each screen implements:
typedef struct {
    const char *title;           // Displayed in header
    void (*init)(void);          // First time entering screen (allocate resources)
    void (*deinit)(void);        // Leaving screen (free resources)
    void (*render)(void);        // Called every DISPLAY_UPDATE_PERIOD_MS
    void (*on_button)(ui_button_event_t event);  // Button press during this screen
    void (*on_timer)(TickType_t elapsed_ms);     // Periodic timer (optional)
} ui_screen_vtable_t;
```

**Transition Flow:**
```
[Bootstrap] → Boot Screen (2s) → Summary (auto-rotate OR manual)
                │
                ├─ SELECT (short)  → Diagnostic (push)
                ├─ SELECT (long)   → Menu (root selector)
                ├─ UP              → Raw
                ├─ DOWN            → Calibration (if factory mode) OR Diagnostic
                │
                └─ In Diagnostic:
                   ├─ UP    → WiFi
                   ├─ DOWN  → MQTT
                   ├─ SELECT → Summary (pop)
                   └─ Menu available via long-press
```

---

## 🔘 Button Handler Design

### Debounce Implementation

**Principles:**
- **Software debounce** (poll-based, not ISR)
- **Sampling period:** 20 ms (50 Hz)
- **Stabilization:** Require N consecutive reads for state change
- **Edge detection:** Falling (press) and rising (release)
- **Timeouts:**
  - **Short press:** < 300 ms → single action
  - **Long press:** ≥ 1000 ms → alternate action / back
  - **Double-click:** Two presses within 300 ms

**State Machine:**
```c
typedef enum {
    UI_BUTTON_STATE_IDLE,
    UI_BUTTON_STATE_DEBOUNCING,
    UI_BUTTON_STATE_PRESSED,
    UI_BUTTON_STATE_WAIT_RELEASE
} ui_button_state_t;

// Per button instance
typedef struct {
    uint8_t stable_level;          // Debounced level (0=pressed, 1=released)
    uint8_t raw_level;             // Last raw read
    uint8_t debounce_counter;      // Counts consecutive debounce matches
    TickType_t last_change_tick;   // Time of last state transition
    ui_button_state_t state;
} ui_button_instance_t;
```

**Algorithm:**
```c
#define UI_BUTTON_DEBOUNCE_TICKS   pdMS_TO_TICKS(20)   // 20 ms sample window
#define UI_BUTTON_PRESS_TICKS      pdMS_TO_TICKS(300)  // Threshold for short press
#define UI_BUTTON_LONG_PRESS_TICKS pdMS_TO_TICKS(1000) // Threshold for long press
#define UI_BUTTON_DOUBLE_CLICK_TICKS pdMS_TO_TICKS(300) // Max interval

// Called every 20-50ms from app loop
void ui_button_poll(void) {
    // 1. Read raw GPIO
    bool raw_pressed = (gpio_read(BUTTON_PIN_UP) == 0);
    uint8_t raw_level = raw_pressed ? 0 : 1;

    // 2. Update debounce counter
    if (raw_level == button->stable_level) {
        button->debounce_counter = DEBOUNCE_COUNT_MAX;  // Reset counter
    } else if (button->debounce_counter > 0) {
        button->debounce_counter--;
    } else {
        // 3. State change confirmed after N mismatches
        button->stable_level = raw_level;
        button->state = (raw_level == 0) ? UI_BUTTON_STATE_PRESSED : UI_BUTTON_STATE_IDLE;
        button->last_change_tick = xTaskGetTickCount();
    }

    // 4. Detect edge (press/release events)
    detect_edges(...);

    // 5. Detect long-press (if pressed > threshold)
    detect_long_press(...);
}
```

**Queue Events:**
```c
typedef struct {
    ui_button_t button;                // UP, DOWN, SELECT
    ui_button_event_type_t type;       // PRESS, RELEASE, LONG_PRESS, DOUBLE_CLICK
    TickType_t timestamp_ms;           // For timing calculations
} ui_button_event_t;

// Producer: ui_button_poll() → xQueueSend(button_event_queue, &event, 0)
// Consumer: app task → xQueueReceive(button_event_queue, &event, portMAX_DELAY)
```

---

## 🖼️ Screen Layout Guidelines

### 128×64 Pixel Grid
```
Page 0 (0-7 px):   Header bar (8px tall)
                    ┌─────────────────────┐
                    │ Title         HEART │  ← 21 chars max
                    ├─────────────────────┤
Page 1 (8-15 px):  Content line 1
Page 2 (16-23):    Content line 2
Page 3 (24-31):    Content line 3
Page 4 (32-39):    Content line 4
Page 5 (40-47):    Content line 5
Page 6 (48-55):    Content line 6
Page 7 (56-63):    Footer/status line
                    └─────────────────────┘
```

### Header Pattern (Fixed)
```
┌───────────────────────────────────┐
│ {title}        {heartbeat:●○◐◑}   │  ← 5×7 font, 21 visible chars max
├───────────────────────────────────┤
```
- **Title:** Screen name (`"SUMMARY"`, `"RAW DATA"`, etc.), max 12 chars
- **Heartbeat:** Animated every 200ms to indicate UI is alive

### Content Guidelines
- **Margins:** 1 character left/right padding
- **Lines:** 6 content lines (pages 1-6)
- **Format:** `"Label: value  units"`, left-aligned
- **Units:** Use minimal labels (`V`, `A`, `W`, `PF`, `Hz`)
- **Warnings:** Invert colors for error states (if supported) or prepend `*` prefix

---

## 📊 Required Screens

### 1. SUMMARY (Main Screen)
```c
void screen_summary_render(void) {
    oled_clear();
    oled_draw_header("RMS VIEW");

    // Line 1: Voltage
    char buf[21]; snprintf(buf, sizeof(buf), "U: %5.1f V", voltage_rms);
    oled_draw_text(1, 0, buf);

    // Line 2: Current
    snprintf(buf, sizeof(buf), "I: %6.3f A", current_rms);
    oled_draw_text(2, 0, buf);

    // Line 3: Power
    snprintf(buf, sizeof(buf), "P: %6.1f W", power_real);
    oled_draw_text(3, 0, buf);

    // Line 4: Power Factor
    snprintf(buf, sizeof(buf), "PF: %5.3f", power_factor);
    oled_draw_text(4, 0, buf);

    // Line 5: WiFi status
    snprintf(buf, sizeof(buf), "WiFi: %s", wifi_state_text);
    oled_draw_text(5, 0, buf);

    // Line 6: MQTT status
    snprintf(buf, sizeof(buf), "MQTT: %s", mqtt_state_text);
    oled_draw_text(6, 0, buf);

    // Footer: heartbeat
    snprintf(buf, sizeof(buf), "%c", get_heartbeat_glyph());
    oled_draw_text(7, 0, buf);
}
```

### 2. RAW DATA
```c
// Shows: ADC raw values + calibration offset/scale
Line 1: "RAW ADC VALUES"
Line 2: "V_raw: %5d     I_raw: %5d"
Line 3: "V_off: %+7.3f  I_off: %+7.3f"
Line 4: "V_scl: %7.3f  I_scl: %7.3f"
Line 5: "Cal state: %s" (FACTORY/FIELD/VALIDATED)
Line 6: (empty or last update time)
Line 7: Footer
```

### 3. DIAGNOSTIC
```c
// Status checklist with [OK] or [FAIL] indicators
Line 1: "SYSTEM STATUS"
Line 2: "[%c] I2C: %s"       (I2C initialized?)
Line 3: "[%c] ADS1015: %s"   (ADC detected?)
Line 4: "[%c] WiFi: %s"      (Connected?)
Line 5: "[%c] MQTT: %s"      (Connected?)
Line 6: "Uptime: %lu s"      (g_stats.uptime_seconds)
Line 7: Footer
```

### 4. WiFi
```c
// WiFi configuration and status
Line 1: "WiFi CONFIG"
Line 2: "SSID: %s" (truncated to 10 chars)
Line 3: "IP: %s"
Line 4: "RSSI: %d dBm"
Line 5: "Mode: %s" (STA/AP/STA+AP)
Line 6: "AP: %s" (soft-AP SSID if active)
Line 7: "SELECT: Reset credentials"
```

### 5. MQTT
```c
Line 1: "MQTT STATUS"
Line 2: "Broker: %s" (truncated)
Line 3: "Port: %d"
Line 4: "State: %s" (CONNECTED/CONNECTING/DISCONNECTED)
Line 5: "Published: %lu"
Line 6: "Failed:   %lu"
Line 7: "SELECT: Reconnect"
```

### 6. CALIBRATION
```c
Line 1: "CALIBRATION"
Line 2: "V: offset=%+.3f" "scale=%6.3f"
Line 3: "I: offset=%+.3f" "scale=%6.3f"
Line 4: "State: %s"
Line 5: "Ref V: %.1fV (set=%d)"
Line 6: "Ref I: %.1fA (set=%d)"
Line 7: "SELECT: Apply current RMS as ref"
```

---

## ⚡ Performance Constraints

### Timing
- **Render period:** 200-500 ms max (do NOT render every loop)
- **Button polling:** Every 20-50 ms
- **Menu navigation:** Instant response (< 100 ms)
- **Screen transition:** Fade or instant (10 ms acceptable)

### Memory
- **Stack per screen render:** ≤ 512 bytes
- **Overall UI stack:** ≤ 2048 bytes (for dedicated UI task)
- **Heap allocations:** Avoid in render path (pre-allocate buffers)
- **Static buffers:** Reuse for each screen render

### CPU
- **OLED I2C transfers:** ~10-30 ms per full screen update
- **Render budget:** < 50 ms per frame (to not interfere with measurement loop)
- **Strategy:** Render to off-screen buffer first → single I2C burst

---

## 🎮 Interaction Patterns

### Navigation Rules (UXGuidelines)
1. **No click-wheel** — 3 distinct buttons only
2. **Single-click:** Select / enter / go forward
3. **Long-press (≥1s):** Back / exit / menu
4. **Double-click:** Toggle auto-rotate mode (summary only)
5. **Button UP/DOWN:** Navigate within list (wraps at boundaries)
6. **No dead-ends:** Every screen can return to Summary via long-press SELECT

### Visual Feedback
- **Heartbeat animation:** Cycle `◯ → ◐ → ◑ → ◒` every 200 ms
- **Screen flash:** Invert for 200 ms on button press (optional)
- **Status indicators:** `[OK]` = green (if color) or "OK", `[ERR]` = red or "ERR"

---

## 🔧 Integration with App Layer

### Public API (`ui_public.h`)
```c
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize UI subsystem
 * @return ESP_OK on success
 */
esp_err_t ui_init(void);

/**
 * @brief Start UI task (if using dedicated task)
 * @return ESP_OK on success
 */
esp_err_t ui_start(void);

/**
 * @brief Set active screen by ID
 * @param[in] screen_id Target screen
 * @return ESP_OK on success
 */
esp_err_t ui_set_active_screen(ui_screen_id_t screen_id);

/**
 * @brief Poll and process pending button events
 * Call this from main app loop if UI runs in same task
 */
void ui_poll_buttons(void);

/**
 * @brief Render current screen content to OLED
 * Call periodically (200-500ms)
 */
void ui_render_current_screen(void);

/**
 * @brief Get pointer to current screen title (for header)
 * @return const char* Screen title string
 */
const char* ui_get_current_screen_title(void);

/**
 * @brief Handle screen rotation timeout
 * Call from app main loop to support auto-rotate
 */
void ui_check_auto_rotate(void);

/**
 * @brief Queue UI event for app consumption
 * Used for button events if app wants to handle them separately
 */
BaseType_t ui_event_queue_send(const ui_button_event_t *event, TickType_t timeout);

/**
 * @brief Receive UI event from app
 */
BaseType_t ui_event_queue_receive(ui_button_event_t *event, TickType_t timeout);

#ifdef __cplusplus
}
#endif
```

### Usage from `energy_analyzer_app.c`
```c
// In energy_analyzer_app_init():
    ret = ui_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "UI init failed: 0x%02x", ret);
    }

// In energy_analyzer_app_run():
    while (1) {
        ui_poll_buttons();                              // 20ms debounce poll
        ui_check_auto_rotate();                         // handle auto-rotate timeout
        if (xTaskGetTickCount() - last_render_tick >= pdMS_TO_TICKS(200)) {
            ui_render_current_screen();                 // render to OLED
            last_render_tick = xTaskGetTickCount();
        }
        vTaskDelay(pdMS_TO_TICKS(20));                  // 20ms loop period
    }
```

---

## 📋 Implementation Checklist

### Phase 1: Infrastructure
- [ ] Create `components/ui/` directory structure
- [ ] Write `ui_public.h` and `ui_types.h`
- [ ] Implement `ui.c` (context, init/deinit)
- [ ] Implement `ui_renderer.c` (OLED abstraction over i2c_hal)
- [ ] Update `main/CMakeLists.txt` - add `ui` component
- [ ] Add `ui` to `app/CMakeLists.txt` `REQUIRES`

### Phase 2: Button Handler
- [ ] Implement `ui_button_handler.c`
- [ ] Per-button debounce state (3 buttons = 3 structs)
- [ ] Polling function (call at 20-50ms interval)
- [ ] Edge detection (press/release)
- [ ] Long-press detection (≥1s)
- [ ] Double-click detection (optional)
- [ ] Button event queue (size 10)
- [ ] Unit test: debounce stability (simulate bouncing signals)

### Phase 3: Screen Framework
- [ ] Implement `screen_base.c/h` — base screen struct
- [ ] Implement `ui_screen_manager.c` — registry, active screen, transition
- [ ] Screen lifecycle: init → enter → render → exit → deinit
- [ ] Back-stack support (for push/pop navigation)
- [ ] Screen discovery/registration (array of `ui_screen_vtable_t`)

### Phase 4: Content Screens
- [ ] `screen_summary.c` — RMS, P, PF, WiFi, MQTT status
- [ ] `screen_raw.c` — raw ADC values, calibration coeffs
- [ ] `screen_diagnostic.c` — system checklist
- [ ] Navigate between screens via buttons
- [ ] Test all transitions

### Phase 5: Configuration Screens
- [ ] `screen_wifi.c` — SSID display, reset button
- [ ] `screen_mqtt.c` — broker config, reconnect action
- [ ] `screen_calibration.c` — view/set voltage/current reference
- [ ] `screen_menu.c` (optional) — menu list navigator

### Phase 6: Integration & Polish
- [ ] Remove OLED code from `energy_analyzer_app.c`
- [ ] Replace all `oled_*` calls with `ui_*` calls
- [ ] Update `app_buttons_init()` and `app_process_buttons()` — remove raw poll
- [ ] Add UI task (optional: separate FreeRTOS task for UI vs same-task)
- [ ] Test on hardware
- [ ] Measure: Avg render time < 50ms, no missed measurement deadlines

---

## 🐛 Known Issues & Gotchas

### Button Debounce
- **Hardware:** Buttons use external pull-ups, active-low
- **No filtering:** Raw reads are susceptible to bouncing (5-20 ms typical)
- **Solution:** Software debounce with 20 ms window, 3+ consecutive samples match

### I2C Bus Sharing
- **Conflict:** ADS1015 (measurement) and OLED share I2C bus
- **Protocol:** I2C is multiplexed on single bus; measurements may block UI
- **Timing:** Measurement takes 10ms every 100ms; OK to interleave
- **Mutex:** `i2c_hal_write_read()` internally uses mutex — no UI-specific action needed

### Render Blocking
- **Risk:** Full-screen clear + 6 lines × ~10 I2C packets = ~200-300 ms worst-case
- **Impact:** Starves measurement task if UI runs in same task
- **Options:**
  1. **Move UI to separate task** (recommended for responsiveness)
  2. **Reduce update rate** → render every 500ms instead of 200ms
  3. **Use partial updates** → only redraw changed lines (diff-based)

---

## 📚 API Reference (for Agent)

### Key Enums
```c
typedef enum {
    UI_BUTTON_UP = 0,
    UI_BUTTON_DOWN,
    UI_BUTTON_SELECT
} ui_button_t;

typedef enum {
    UI_BUTTON_EVT_PRESS = 0,      // Falling edge (press)
    UI_BUTTON_EVT_RELEASE,        // Rising edge (release)
    UI_BUTTON_EVT_LONG_PRESS,     // Held >= 1 second
    UI_BUTTON_EVT_DOUBLE_CLICK    // Pressed twice rapidly
} ui_button_event_type_t;

typedef enum {
    UI_SCREEN_SUMMARY = 0,
    UI_SCREEN_RAW,
    UI_SCREEN_DIAGNOSTIC,
    UI_SCREEN_WIFI,
    UI_SCREEN_MQTT,
    UI_SCREEN_CALIBRATION,
    UI_SCREEN_MENU,
    UI_SCREEN_COUNT
} ui_screen_id_t;
```

### Key Structs
```c
typedef struct {
    ui_button_t button;
    ui_button_event_type_t type;
    TickType_t timestamp_ms;
} ui_button_event_t;

typedef struct {
    const char *title;
    void (*init)(void);
    void (*deinit)(void);
    void (*render)(void);
    void (*on_button)(ui_button_event_t);
    void (*on_timer)(TickType_t);
} ui_screen_vtable_t;
```

### Core Functions
```c
esp_err_t ui_init(void);
esp_err_t ui_start(void);
esp_err_t ui_set_active_screen(ui_screen_id_t screen_id);
void ui_poll_buttons(void);
void ui_render_current_screen(void);
void ui_check_auto_rotate(void);
```

---

## 🎯 Success Criteria

1. **Navigation:** Button press → screen change ≤ 100 ms
2. **Responsiveness:** Long-press detected within 100 ms of threshold
3. **Debounce:** No false triggers with 5-20 ms hardware bounce
4. **Render time:** Full screen ≤ 50 ms average (≤ 25% of measurement loop)
5. **Memory:** UI stack ≤ 2 KB, no heap fragmentation over 24h
6. **Code quality:** Zero warnings, MISRA C, complete Doxygen

---

## 📖 Reference Implementations

Look at these existing patterns:
- **Button handling pattern:** `components/app/energy_analyzer_app.c:app_process_buttons()`
- **OLED driver pattern:** `components/app/energy_analyzer_app.c:oled_*` functions
- **Screen switching pattern:** `APP_SCREEN_RAW` / `APP_SCREEN_SUMMARY` enum + `g_active_screen`
- **Heartbeat animation:** `app_get_heartbeat_glyph()` — returns `0,1,2,3` cyclically
- **Debounce constants:** `BUTTON_DEBOUNCE_MS` in `hardware_config.h` (currently 20ms)

---

**UI Design Principles:**
- **Clean** — Separation of display, input, and logic
- **Fast** — Render quickly, non-blocking
- **Intuitive** — Consistent navigation, clear labeling
- **Maintainable** — Each screen is isolated module

**Let's build a beautiful embedded UI! 🎛️**
