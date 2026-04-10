/**
 * @file energy_analyzer_app.c
 * @brief Main application controller implementation
 */

#include "energy_analyzer_app.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// HAL includes
#include "ads1015_hal.h"
#include "hal.h"
#include "i2c_hal.h"

// Services includes
#include "measurement_service.h"
#include "app_mqtt_client.h"
#include "network.h"

// Config includes
#include "hardware_config.h"
#include "timing_config.h"

static const char *TAG = "ENERGY_APP";

#define OLED_WIDTH_PIXELS          128U
#define OLED_PAGE_COUNT            8U
#define OLED_CONTROL_BYTE_CMD      0x00U
#define OLED_CONTROL_BYTE_DATA     0x40U
#define OLED_GLYPH_WIDTH           5U
#define OLED_GLYPH_SPACING         1U
#define OLED_STATUS_TEXT_LENGTH    21U
#define RAW_LOG_PERIOD_MS          1000U
#define RMS_LOG_PERIOD_MS          5000U
#define UI_FILTER_SHIFT            2U
#define MEASUREMENT_STALE_TIMEOUT_MS 2000U
#define BUTTON_TRACE_PERIOD_MS     2000U

typedef enum {
    APP_SCREEN_RAW = 0,
    APP_SCREEN_PANEL = 1,
    APP_SCREEN_SUMMARY = 2,
    APP_SCREEN_STATE = 3,
    APP_SCREEN_COUNT
} app_screen_t;

typedef enum {
    APP_MENU_GENERAL = 0,
    APP_MENU_SERVICES = 1,
    APP_MENU_METRICS = 2,
    APP_MENU_SENSOR = 3,
    APP_MENU_COUNT
} app_menu_item_t;

static bool g_oled_ready = false;
static bool g_ads1015_hal_ready = false;
static bool g_measurement_ready = false;
static uint8_t g_oled_back_buffer[OLED_PAGE_COUNT][OLED_WIDTH_PIXELS] = {0};
static uint8_t g_oled_front_buffer[OLED_PAGE_COUNT][OLED_WIDTH_PIXELS] = {0};
static bool g_oled_front_buffer_valid = false;
static wifi_service_status_t g_wifi_status = {
    .state = WIFI_STATE_DISCONNECTED,
    .ap_active = false,
    .credentials_saved = false,
    .ap_ssid = {0},
    .sta_ssid = {0},
    .ip_address = "0.0.0.0"
};
static mqtt_client_status_t g_mqtt_status = {
    .state = MQTT_STATE_DISCONNECTED,
    .broker_url = {0},
    .connected_at_ms = 0,
    .messages_published = 0,
    .messages_failed = 0,
    .last_published_ms = 0,
    .auto_reconnect_enabled = false
};
static app_screen_t g_active_screen = APP_SCREEN_RAW;
static bool g_menu_active = true;
static app_menu_item_t g_menu_selection = APP_MENU_GENERAL;
static measurement_result_t g_last_measurement = {0};
static bool g_last_measurement_valid = false;
static TickType_t g_last_mqtt_publish_tick = 0U;
static TickType_t g_last_measurement_valid_tick = 0U;
static TickType_t g_last_rms_log_tick = 0U;
static bool g_mqtt_runtime_started = false;

typedef struct {
    int16_t voltage_raw;
    int16_t current_raw;
    int16_t voltage_display_raw;
    int16_t current_display_raw;
    esp_err_t voltage_status;
    esp_err_t current_status;
    bool voltage_valid;
    bool current_valid;
} app_ads1015_diag_t;

typedef struct {
    uint8_t up_level;
    uint8_t down_level;
    uint8_t select_level;
    TickType_t up_last_press_tick;
    TickType_t down_last_press_tick;
    TickType_t select_last_press_tick;
} app_button_state_t;

typedef struct {
    app_screen_t active_screen;
    bool menu_active;
    app_menu_item_t menu_selection;
    bool measurement_valid;
    int voltage_tenths;
    int current_hundredths;
    int power_tenths;
    int pf_hundredths;
    uint8_t wifi_state;
    uint8_t mqtt_state;
    bool ap_active;
    bool ads_voltage_valid;
    bool ads_current_valid;
    int voltage_raw;
    int current_raw;
    unsigned long messages_published;
} app_display_snapshot_t;

static app_ads1015_diag_t g_ads_diag = {
    .voltage_raw = 0,
    .current_raw = 0,
    .voltage_display_raw = 0,
    .current_display_raw = 0,
    .voltage_status = ESP_ERR_INVALID_STATE,
    .current_status = ESP_ERR_INVALID_STATE,
    .voltage_valid = false,
    .current_valid = false
};

static app_button_state_t g_button_state = {
    .up_level = 1U,
    .down_level = 1U,
    .select_level = 1U,
    .up_last_press_tick = 0U,
    .down_last_press_tick = 0U,
    .select_last_press_tick = 0U
};

static esp_err_t oled_write_command(uint8_t command);
static esp_err_t oled_write_data(const uint8_t *data, size_t length);
static esp_err_t oled_init(void);
static esp_err_t oled_clear(void);
static esp_err_t oled_flush(void);
static esp_err_t oled_set_cursor(uint8_t page, uint8_t column);
static esp_err_t oled_draw_text(uint8_t page, uint8_t column, const char *text);
static esp_err_t oled_draw_pattern_span(uint8_t page,
                                        uint8_t column,
                                        uint8_t width,
                                        uint8_t pattern);
static esp_err_t oled_draw_menu_marker(uint8_t page, uint8_t column, bool selected);
static esp_err_t app_buttons_init(void);
static void app_format_raw_line(char *buffer,
                                size_t buffer_length,
                                char channel_name,
                                bool valid,
                                int16_t raw_value,
                                esp_err_t status);
static void app_format_measurement_line(char *buffer,
                                        size_t buffer_length,
                                        char label,
                                        float value,
                                        uint8_t decimals);
static const char *app_get_short_error_text(esp_err_t status);
static void app_process_buttons(void);
static void app_log_button_trace(uint8_t up_level,
                                 uint8_t down_level,
                                 uint8_t select_level,
                                 bool force_log);
static void app_log_calibration_help(void);
static void app_log_current_calibration(void);
static void app_log_wifi_help(void);
static void app_log_current_wifi_status(void);
static void app_refresh_wifi_status(void);
static void app_log_ads1015_status(TickType_t *last_log_tick);
static void app_read_ads1015_raw(void);
static void app_update_measurement_health(void);
static void app_render_panel_screen(void);
static void app_render_main_menu_screen(void);
static void app_render_summary_screen(void);
static void app_render_raw_screen(void);
static void app_render_state_screen(void);
static void app_render_diagnostic_screen(void);
static const char *app_get_screen_title(app_screen_t screen);
static const char *app_get_menu_title(app_menu_item_t item);
static void app_render_header(const char *title);
static void app_render_footer(const char *text);
static void app_cycle_screen(int8_t direction);
static void app_cycle_menu(int8_t direction);
static void app_open_main_menu(void);
static void app_activate_menu_selection(void);
static const char *app_get_rotation_state_text(void);
static const char *app_get_ads_state_text(void);
static const char *app_get_measurement_state_text(void);
static const char *app_get_wifi_state_text(void);
static const char *app_get_mqtt_state_text(void);
static void app_refresh_mqtt_status(void);
static bool app_wifi_ready_for_mqtt(void);
static void app_manage_mqtt_runtime(void);
static void app_log_mqtt_help(void);
static void app_log_current_mqtt_status(void);
static void app_format_panel_metric(char *buffer,
                                    size_t buffer_length,
                                    const char *unit,
                                    float value,
                                    uint8_t decimals);
static void app_render_horizontal_rule(uint8_t page);
static void app_render_centered_text(uint8_t page,
                                     uint8_t column,
                                     uint8_t width,
                                     const char *text);
static app_display_snapshot_t app_capture_display_snapshot(void);
static bool app_display_snapshot_changed(const app_display_snapshot_t *current,
                                         const app_display_snapshot_t *previous);
static const uint8_t *oled_get_glyph(char character);

static esp_err_t oled_write_command(uint8_t command)
{
    uint8_t payload[2] = { OLED_CONTROL_BYTE_CMD, command };

    return i2c_hal_write_read(CONFIG_I2C_PORT,
                              OLED_I2C_ADDRESS,
                              payload,
                              sizeof(payload),
                              NULL,
                              0U);
}

static esp_err_t oled_write_data(const uint8_t *data, size_t length)
{
    uint8_t payload[17];
    size_t offset = 0U;

    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    while (offset < length) {
        size_t chunk = length - offset;
        if (chunk > (sizeof(payload) - 1U)) {
            chunk = sizeof(payload) - 1U;
        }

        payload[0] = OLED_CONTROL_BYTE_DATA;
        memcpy(&payload[1], &data[offset], chunk);

        esp_err_t ret = i2c_hal_write_read(CONFIG_I2C_PORT,
                                           OLED_I2C_ADDRESS,
                                           payload,
                                           chunk + 1U,
                                           NULL,
                                           0U);
        if (ret != ESP_OK) {
            return ret;
        }

        offset += chunk;
    }

    return ESP_OK;
}

static esp_err_t oled_set_cursor(uint8_t page, uint8_t column)
{
    esp_err_t ret;

    ret = oled_write_command((uint8_t)(0xB0U + page));
    if (ret != ESP_OK) {
        return ret;
    }

    ret = oled_write_command((uint8_t)(0x00U + (column & 0x0FU)));
    if (ret != ESP_OK) {
        return ret;
    }

    ret = oled_write_command((uint8_t)(0x10U + ((column >> 4U) & 0x0FU)));
    return ret;
}

static esp_err_t oled_clear(void)
{
    memset(g_oled_back_buffer, 0, sizeof(g_oled_back_buffer));
    return ESP_OK;
}

static esp_err_t oled_flush(void)
{
    for (uint8_t page = 0U; page < OLED_PAGE_COUNT; ++page) {
        if (g_oled_front_buffer_valid &&
            (memcmp(g_oled_front_buffer[page], g_oled_back_buffer[page], OLED_WIDTH_PIXELS) == 0)) {
            continue;
        }

        esp_err_t ret = oled_set_cursor(page, 0U);
        if (ret != ESP_OK) {
            return ret;
        }

        ret = oled_write_data(g_oled_back_buffer[page], OLED_WIDTH_PIXELS);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    memcpy(g_oled_front_buffer, g_oled_back_buffer, sizeof(g_oled_front_buffer));
    g_oled_front_buffer_valid = true;
    return ESP_OK;
}

static esp_err_t oled_draw_pattern_span(uint8_t page,
                                        uint8_t column,
                                        uint8_t width,
                                        uint8_t pattern)
{
    if ((page >= OLED_PAGE_COUNT) || (column >= OLED_WIDTH_PIXELS) || (width == 0U)) {
        return ESP_ERR_INVALID_ARG;
    }

    if ((uint16_t)column + (uint16_t)width > OLED_WIDTH_PIXELS) {
        width = (uint8_t)(OLED_WIDTH_PIXELS - column);
    }

    memset(&g_oled_back_buffer[page][column], pattern, width);
    return ESP_OK;
}

static esp_err_t oled_draw_menu_marker(uint8_t page, uint8_t column, bool selected)
{
    static const uint8_t marker_selected[OLED_GLYPH_WIDTH + OLED_GLYPH_SPACING] = {
        0x08U, 0x1CU, 0x3EU, 0x1CU, 0x08U, 0x00U
    };
    static const uint8_t marker_empty[OLED_GLYPH_WIDTH + OLED_GLYPH_SPACING] = {
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
    };
    const uint8_t *marker = selected ? marker_selected : marker_empty;

    if ((page >= OLED_PAGE_COUNT) || (column >= OLED_WIDTH_PIXELS)) {
        return ESP_ERR_INVALID_ARG;
    }

    if ((uint16_t)column + (OLED_GLYPH_WIDTH + OLED_GLYPH_SPACING) > OLED_WIDTH_PIXELS) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&g_oled_back_buffer[page][column], marker, OLED_GLYPH_WIDTH + OLED_GLYPH_SPACING);
    return ESP_OK;
}

static esp_err_t oled_init(void)
{
    static const uint8_t init_commands[] = {
        0xAEU, 0xD5U, 0x80U, 0xA8U, 0x3FU, 0xD3U, 0x00U, 0x40U,
        0x8DU, 0x14U, 0x20U, 0x00U, 0xA1U, 0xC8U, 0xDAU, 0x12U,
        0x81U, 0xCFU, 0xD9U, 0xF1U, 0xDBU, 0x40U, 0xA4U, 0xA6U,
        0x2EU, 0xAFU
    };

    for (size_t index = 0U; index < sizeof(init_commands); ++index) {
        esp_err_t ret = oled_write_command(init_commands[index]);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    (void)oled_clear();
    memset(g_oled_front_buffer, 0, sizeof(g_oled_front_buffer));
    g_oled_front_buffer_valid = false;
    return oled_flush();
}

static esp_err_t app_buttons_init(void)
{
    TickType_t current_tick = xTaskGetTickCount();
    esp_err_t ret;
    bool pressed = false;

    ret = board_buttons_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Button GPIO config failed: 0x%02x", ret);
        return ret;
    }

    ret = board_button_read(BUTTON_PIN_UP, &pressed);
    if (ret != ESP_OK) {
        return ret;
    }
    g_button_state.up_level = pressed ? 0U : 1U;

    ret = board_button_read(BUTTON_PIN_DOWN, &pressed);
    if (ret != ESP_OK) {
        return ret;
    }
    g_button_state.down_level = pressed ? 0U : 1U;

    ret = board_button_read(BUTTON_PIN_SELECT, &pressed);
    if (ret != ESP_OK) {
        return ret;
    }
    g_button_state.select_level = pressed ? 0U : 1U;

    g_button_state.up_last_press_tick = current_tick;
    g_button_state.down_last_press_tick = current_tick;
    g_button_state.select_last_press_tick = current_tick;

    ESP_LOGI(TAG, "Button idle levels: UP=%u DOWN=%u SELECT=%u",
             (unsigned int)g_button_state.up_level,
             (unsigned int)g_button_state.down_level,
             (unsigned int)g_button_state.select_level);
    ESP_LOGI(TAG, "Button GPIO mapping: UP=%d DOWN=%d SELECT=%d",
             (int)BUTTON_PIN_UP,
             (int)BUTTON_PIN_DOWN,
             (int)BUTTON_PIN_SELECT);

    if ((g_button_state.up_level == 0U) ||
        (g_button_state.down_level == 0U) ||
        (g_button_state.select_level == 0U)) {
        ESP_LOGW(TAG, "One or more buttons are active during boot; check pull-up, wiring, and selected GPIOs");
    }

    return ESP_OK;
}

static const uint8_t *oled_get_glyph(char character)
{
    static const uint8_t glyph_space[OLED_GLYPH_WIDTH] = { 0x00U, 0x00U, 0x00U, 0x00U, 0x00U };
    static const uint8_t glyph_dash[OLED_GLYPH_WIDTH]  = { 0x08U, 0x08U, 0x08U, 0x08U, 0x08U };
    static const uint8_t glyph_colon[OLED_GLYPH_WIDTH] = { 0x00U, 0x36U, 0x36U, 0x00U, 0x00U };
    static const uint8_t glyph_dot[OLED_GLYPH_WIDTH]   = { 0x00U, 0x60U, 0x60U, 0x00U, 0x00U };
    static const uint8_t glyph_percent[OLED_GLYPH_WIDTH] = { 0x63U, 0x13U, 0x08U, 0x64U, 0x63U };
    static const uint8_t glyph_0[OLED_GLYPH_WIDTH]     = { 0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU };
    static const uint8_t glyph_1[OLED_GLYPH_WIDTH]     = { 0x00U, 0x42U, 0x7FU, 0x40U, 0x00U };
    static const uint8_t glyph_2[OLED_GLYPH_WIDTH]     = { 0x42U, 0x61U, 0x51U, 0x49U, 0x46U };
    static const uint8_t glyph_3[OLED_GLYPH_WIDTH]     = { 0x21U, 0x41U, 0x45U, 0x4BU, 0x31U };
    static const uint8_t glyph_4[OLED_GLYPH_WIDTH]     = { 0x18U, 0x14U, 0x12U, 0x7FU, 0x10U };
    static const uint8_t glyph_5[OLED_GLYPH_WIDTH]     = { 0x27U, 0x45U, 0x45U, 0x45U, 0x39U };
    static const uint8_t glyph_6[OLED_GLYPH_WIDTH]     = { 0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U };
    static const uint8_t glyph_7[OLED_GLYPH_WIDTH]     = { 0x01U, 0x71U, 0x09U, 0x05U, 0x03U };
    static const uint8_t glyph_8[OLED_GLYPH_WIDTH]     = { 0x36U, 0x49U, 0x49U, 0x49U, 0x36U };
    static const uint8_t glyph_9[OLED_GLYPH_WIDTH]     = { 0x06U, 0x49U, 0x49U, 0x29U, 0x1EU };
    static const uint8_t glyph_A[OLED_GLYPH_WIDTH]     = { 0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU };
    static const uint8_t glyph_C[OLED_GLYPH_WIDTH]     = { 0x3EU, 0x41U, 0x41U, 0x41U, 0x22U };
    static const uint8_t glyph_D[OLED_GLYPH_WIDTH]     = { 0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU };
    static const uint8_t glyph_E[OLED_GLYPH_WIDTH]     = { 0x7FU, 0x49U, 0x49U, 0x49U, 0x41U };
    static const uint8_t glyph_F[OLED_GLYPH_WIDTH]     = { 0x7FU, 0x09U, 0x09U, 0x09U, 0x01U };
    static const uint8_t glyph_H[OLED_GLYPH_WIDTH]     = { 0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU };
    static const uint8_t glyph_I[OLED_GLYPH_WIDTH]     = { 0x00U, 0x41U, 0x7FU, 0x41U, 0x00U };
    static const uint8_t glyph_K[OLED_GLYPH_WIDTH]     = { 0x7FU, 0x08U, 0x14U, 0x22U, 0x41U };
    static const uint8_t glyph_L[OLED_GLYPH_WIDTH]     = { 0x7FU, 0x40U, 0x40U, 0x40U, 0x40U };
    static const uint8_t glyph_M[OLED_GLYPH_WIDTH]     = { 0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU };
    static const uint8_t glyph_N[OLED_GLYPH_WIDTH]     = { 0x7FU, 0x04U, 0x08U, 0x10U, 0x7FU };
    static const uint8_t glyph_O[OLED_GLYPH_WIDTH]     = { 0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU };
    static const uint8_t glyph_P[OLED_GLYPH_WIDTH]     = { 0x7FU, 0x09U, 0x09U, 0x09U, 0x06U };
    static const uint8_t glyph_Q[OLED_GLYPH_WIDTH]     = { 0x3EU, 0x41U, 0x51U, 0x21U, 0x5EU };
    static const uint8_t glyph_R[OLED_GLYPH_WIDTH]     = { 0x7FU, 0x09U, 0x19U, 0x29U, 0x46U };
    static const uint8_t glyph_S[OLED_GLYPH_WIDTH]     = { 0x46U, 0x49U, 0x49U, 0x49U, 0x31U };
    static const uint8_t glyph_T[OLED_GLYPH_WIDTH]     = { 0x01U, 0x01U, 0x7FU, 0x01U, 0x01U };
    static const uint8_t glyph_U[OLED_GLYPH_WIDTH]     = { 0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU };
    static const uint8_t glyph_V[OLED_GLYPH_WIDTH]     = { 0x1FU, 0x20U, 0x40U, 0x20U, 0x1FU };
    static const uint8_t glyph_W[OLED_GLYPH_WIDTH]     = { 0x7FU, 0x20U, 0x18U, 0x20U, 0x7FU };
    static const uint8_t glyph_X[OLED_GLYPH_WIDTH]     = { 0x63U, 0x14U, 0x08U, 0x14U, 0x63U };
    static const uint8_t glyph_Y[OLED_GLYPH_WIDTH]     = { 0x03U, 0x04U, 0x78U, 0x04U, 0x03U };

    switch (character) {
        case ' ': return glyph_space;
        case '-': return glyph_dash;
        case ':': return glyph_colon;
        case '.': return glyph_dot;
        case '%': return glyph_percent;
        case '0': return glyph_0;
        case '1': return glyph_1;
        case '2': return glyph_2;
        case '3': return glyph_3;
        case '4': return glyph_4;
        case '5': return glyph_5;
        case '6': return glyph_6;
        case '7': return glyph_7;
        case '8': return glyph_8;
        case '9': return glyph_9;
        case 'A': return glyph_A;
        case 'C': return glyph_C;
        case 'D': return glyph_D;
        case 'E': return glyph_E;
        case 'F': return glyph_F;
        case 'H': return glyph_H;
        case 'I': return glyph_I;
        case 'K': return glyph_K;
        case 'L': return glyph_L;
        case 'M': return glyph_M;
        case 'N': return glyph_N;
        case 'O': return glyph_O;
        case 'P': return glyph_P;
        case 'Q': return glyph_Q;
        case 'R': return glyph_R;
        case 'S': return glyph_S;
        case 'T': return glyph_T;
        case 'U': return glyph_U;
        case 'V': return glyph_V;
        case 'W': return glyph_W;
        case 'X': return glyph_X;
        case 'Y': return glyph_Y;
        default:  return glyph_space;
    }
}

static esp_err_t oled_draw_text(uint8_t page, uint8_t column, const char *text)
{
    uint8_t current_column = column;

    if ((text == NULL) || (page >= OLED_PAGE_COUNT) || (column >= OLED_WIDTH_PIXELS)) {
        return ESP_ERR_INVALID_ARG;
    }

    while ((*text != '\0') && (current_column < (OLED_WIDTH_PIXELS - OLED_GLYPH_WIDTH))) {
        const uint8_t *glyph = oled_get_glyph(*text);

        memcpy(&g_oled_back_buffer[page][current_column], glyph, OLED_GLYPH_WIDTH);
        if ((current_column + OLED_GLYPH_WIDTH) < OLED_WIDTH_PIXELS) {
            g_oled_back_buffer[page][current_column + OLED_GLYPH_WIDTH] = 0x00U;
        }

        current_column = (uint8_t)(current_column + OLED_GLYPH_WIDTH + OLED_GLYPH_SPACING);
        ++text;
    }

    return ESP_OK;
}

static const char *app_get_short_error_text(esp_err_t status)
{
    switch (status) {
        case ESP_ERR_TIMEOUT:
            return "TIME";
        case ESP_ERR_INVALID_STATE:
            return "INIT";
        default:
            return "ERR";
    }
}

static void app_format_raw_line(char *buffer,
                                size_t buffer_length,
                                char channel_name,
                                bool valid,
                                int16_t raw_value,
                                esp_err_t status)
{
    if ((buffer == NULL) || (buffer_length == 0U)) {
        return;
    }

    if (valid) {
        (void)snprintf(buffer, buffer_length, "%cRAW:%5d", channel_name, (int)raw_value);
    } else {
        (void)snprintf(buffer, buffer_length, "%cRAW:%s",
                       channel_name,
                       app_get_short_error_text(status));
    }
}

static void app_log_button_trace(uint8_t up_level,
                                 uint8_t down_level,
                                 uint8_t select_level,
                                 bool force_log)
{
    static uint8_t s_last_up_level = 1U;
    static uint8_t s_last_down_level = 1U;
    static uint8_t s_last_select_level = 1U;
    static TickType_t s_last_trace_tick = 0U;
    TickType_t current_tick = xTaskGetTickCount();
    bool changed = (up_level != s_last_up_level) ||
                   (down_level != s_last_down_level) ||
                   (select_level != s_last_select_level);

    if (!force_log &&
        !changed &&
        ((current_tick - s_last_trace_tick) < pdMS_TO_TICKS(BUTTON_TRACE_PERIOD_MS))) {
        return;
    }

    s_last_up_level = up_level;
    s_last_down_level = down_level;
    s_last_select_level = select_level;
    s_last_trace_tick = current_tick;

    ESP_LOGI(TAG, "Buttons raw: UP=%u DOWN=%u SELECT=%u",
             (unsigned int)up_level,
             (unsigned int)down_level,
             (unsigned int)select_level);
}

static void app_format_measurement_line(char *buffer,
                                        size_t buffer_length,
                                        char label,
                                        float value,
                                        uint8_t decimals)
{
    if ((buffer == NULL) || (buffer_length == 0U)) {
        return;
    }

    if (decimals == 0U) {
        (void)snprintf(buffer, buffer_length, "%c:%4d", label, (int)value);
    } else {
        (void)snprintf(buffer, buffer_length, "%c:%4.*f", label, (int)decimals, (double)value);
    }
}

static void app_process_buttons(void)
{
    bool up_pressed = false;
    bool down_pressed = false;
    bool select_pressed = false;
    uint8_t up_level;
    uint8_t down_level;
    uint8_t select_level;
    TickType_t current_tick = xTaskGetTickCount();
    TickType_t debounce_ticks = pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS);

    if (board_button_read(BUTTON_PIN_UP, &up_pressed) != ESP_OK) {
        return;
    }

    if (board_button_read(BUTTON_PIN_DOWN, &down_pressed) != ESP_OK) {
        return;
    }

    if (board_button_read(BUTTON_PIN_SELECT, &select_pressed) != ESP_OK) {
        return;
    }

    up_level = up_pressed ? 0U : 1U;
    down_level = down_pressed ? 0U : 1U;
    select_level = select_pressed ? 0U : 1U;

    app_log_button_trace(up_level, down_level, select_level, false);

    if ((g_button_state.select_level != 0U) &&
        (select_level == 0U) &&
        ((current_tick - g_button_state.select_last_press_tick) >= debounce_ticks)) {
        g_button_state.select_last_press_tick = current_tick;
        if (g_menu_active) {
            app_activate_menu_selection();
            ESP_LOGI(TAG, "Button SELECT pressed -> open screen=%s", app_get_screen_title(g_active_screen));
        } else {
            app_open_main_menu();
            ESP_LOGI(TAG, "Button SELECT pressed -> main menu");
        }
    }

    if ((g_button_state.up_level != 0U) &&
        (up_level == 0U) &&
        ((current_tick - g_button_state.up_last_press_tick) >= debounce_ticks)) {
        g_button_state.up_last_press_tick = current_tick;
        if (g_menu_active) {
            app_cycle_menu(-1);
            ESP_LOGI(TAG, "Button UP pressed -> menu=%s", app_get_menu_title(g_menu_selection));
        } else {
            app_cycle_screen(-1);
            ESP_LOGI(TAG, "Button UP pressed -> screen=%s", app_get_screen_title(g_active_screen));
        }
    }

    if ((g_button_state.down_level != 0U) &&
        (down_level == 0U) &&
        ((current_tick - g_button_state.down_last_press_tick) >= debounce_ticks)) {
        g_button_state.down_last_press_tick = current_tick;
        if (g_menu_active) {
            app_cycle_menu(1);
            ESP_LOGI(TAG, "Button DOWN pressed -> menu=%s", app_get_menu_title(g_menu_selection));
        } else {
            app_cycle_screen(1);
            ESP_LOGI(TAG, "Button DOWN pressed -> screen=%s", app_get_screen_title(g_active_screen));
        }
    }

    g_button_state.up_level = up_level;
    g_button_state.down_level = down_level;
    g_button_state.select_level = select_level;
}

static const char *app_get_screen_title(app_screen_t screen)
{
    switch (screen) {
        case APP_SCREEN_PANEL:
            return "PANEL";
        case APP_SCREEN_SUMMARY:
            return "RMS";
        case APP_SCREEN_STATE:
            return "STATE";
        case APP_SCREEN_RAW:
        default:
            return "RAW";
    }
}

static const char *app_get_menu_title(app_menu_item_t item)
{
    switch (item) {
        case APP_MENU_GENERAL:
            return "PAINEL";
        case APP_MENU_SERVICES:
            return "SERVICOS";
        case APP_MENU_METRICS:
            return "METRICAS";
        case APP_MENU_SENSOR:
        default:
            return "SENSOR";
    }
}

static void app_render_header(const char *title)
{
    if (title == NULL) {
        title = "APP";
    }

    (void)oled_draw_text(0U, 0U, title);
}

static void app_render_footer(const char *text)
{
    char footer_text[OLED_STATUS_TEXT_LENGTH];

    if (text == NULL) {
        text = "";
    }

    (void)snprintf(footer_text, sizeof(footer_text), "%s", text);
    (void)oled_draw_text(7U, 0U, footer_text);
}

static void app_cycle_screen(int8_t direction)
{
    int next_screen = (int)g_active_screen + (int)direction;

    if (next_screen < 0) {
        next_screen = (int)APP_SCREEN_COUNT - 1;
    } else if (next_screen >= (int)APP_SCREEN_COUNT) {
        next_screen = 0;
    }

    g_active_screen = (app_screen_t)next_screen;
}

static void app_cycle_menu(int8_t direction)
{
    int next_menu = (int)g_menu_selection + (int)direction;

    if (next_menu < 0) {
        next_menu = (int)APP_MENU_COUNT - 1;
    } else if (next_menu >= (int)APP_MENU_COUNT) {
        next_menu = 0;
    }

    g_menu_selection = (app_menu_item_t)next_menu;
}

static void app_open_main_menu(void)
{
    switch (g_active_screen) {
        case APP_SCREEN_PANEL:
            g_menu_selection = APP_MENU_GENERAL;
            break;
        case APP_SCREEN_STATE:
            g_menu_selection = APP_MENU_SERVICES;
            break;
        case APP_SCREEN_SUMMARY:
            g_menu_selection = APP_MENU_METRICS;
            break;
        case APP_SCREEN_RAW:
        default:
            g_menu_selection = APP_MENU_SENSOR;
            break;
    }

    g_menu_active = true;
}

static void app_activate_menu_selection(void)
{
    switch (g_menu_selection) {
        case APP_MENU_GENERAL:
            g_active_screen = APP_SCREEN_PANEL;
            break;
        case APP_MENU_SERVICES:
            g_active_screen = APP_SCREEN_STATE;
            break;
        case APP_MENU_METRICS:
            g_active_screen = APP_SCREEN_SUMMARY;
            break;
        case APP_MENU_SENSOR:
        default:
            g_active_screen = APP_SCREEN_RAW;
            break;
    }

    g_menu_active = false;
}

static const char *app_get_rotation_state_text(void)
{
    return "MAN";
}

static void app_log_calibration_help(void)
{
    ESP_LOGI(TAG, "Serial calibration commands:");
    ESP_LOGI(TAG, "  CAL SHOW");
    ESP_LOGI(TAG, "  CAL V <reference_voltage_rms>");
    ESP_LOGI(TAG, "  CAL I <reference_current_rms>");
    ESP_LOGI(TAG, "  CAL SAVE");
    ESP_LOGI(TAG, "  CAL RESET");
}

static void app_log_wifi_help(void)
{
    ESP_LOGI(TAG, "Serial Wi-Fi commands:");
    ESP_LOGI(TAG, "  WIFI SHOW");
    ESP_LOGI(TAG, "  WIFI RESET");
}

static void app_log_current_calibration(void)
{
    field_calibration_t calibration = {0};

    if (measurement_service_get_field_calibration(&calibration) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read field calibration");
        return;
    }

    ESP_LOGI(TAG,
             "Calibration state=%d V(offset=%.3f scale=%.3f) I(offset=%.3f scale=%.3f)",
             (int)calibration.state,
             calibration.coefficients.voltage_offset,
             calibration.coefficients.voltage_scale,
             calibration.coefficients.current_offset,
             calibration.coefficients.current_scale);
}

static const char *app_get_wifi_state_text(void)
{
    switch (g_wifi_status.state) {
        case WIFI_STATE_PROVISIONING:
            return "AP";
        case WIFI_STATE_CONNECTING:
            return "RETRY";
        case WIFI_STATE_CONNECTED:
            return "IP";
        case WIFI_STATE_ERROR:
            return "ERR";
        default:
            return "IDLE";
    }
}

static void app_log_current_wifi_status(void)
{
    ESP_LOGI(TAG,
             "Wi-Fi state=%s ap=%d saved=%d ssid='%s' ip=%s portal='%s'",
             app_get_wifi_state_text(),
             g_wifi_status.ap_active ? 1 : 0,
             g_wifi_status.credentials_saved ? 1 : 0,
             g_wifi_status.credentials_saved ? g_wifi_status.sta_ssid : "-",
             g_wifi_status.ip_address,
             g_wifi_status.ap_ssid[0] != '\0' ? g_wifi_status.ap_ssid : "-");
}

static void app_refresh_wifi_status(void)
{
    if (wifi_service_get_status(&g_wifi_status) != ESP_OK) {
        g_wifi_status.state = WIFI_STATE_ERROR;
        g_wifi_status.ap_active = false;
        g_wifi_status.credentials_saved = false;
        SAFE_STRCPY(g_wifi_status.ip_address, "0.0.0.0", sizeof(g_wifi_status.ip_address));
    }
}

static const char *app_get_mqtt_state_text(void)
{
    switch (g_mqtt_status.state) {
        case MQTT_STATE_CONNECTING:
            return "WAIT";
        case MQTT_STATE_CONNECTED:
            return "OK";
        case MQTT_STATE_ERROR:
            return "ERR";
        default:
            return "OFF";
    }
}

static const char *app_get_ads_state_text(void)
{
    if (g_ads_diag.voltage_valid && g_ads_diag.current_valid) {
        return "OK";
    }

    if (g_ads1015_hal_ready) {
        return "DEG";
    }

    return "ERR";
}

static const char *app_get_measurement_state_text(void)
{
    if (g_last_measurement_valid) {
        return "OK";
    }

    if (g_ads_diag.voltage_valid || g_ads_diag.current_valid) {
        return "WAIT";
    }

    return "DEG";
}

static void app_refresh_mqtt_status(void)
{
    g_mqtt_status = mqtt_client_get_status();
}

static bool app_wifi_ready_for_mqtt(void)
{
    return g_wifi_status.state == WIFI_STATE_CONNECTED;
}

static void app_manage_mqtt_runtime(void)
{
    if (app_wifi_ready_for_mqtt()) {
        if (!g_mqtt_runtime_started) {
            esp_err_t ret = mqtt_client_start(NULL);
            if (ret == ESP_OK) {
                g_mqtt_runtime_started = true;
                ESP_LOGI(TAG, "MQTT runtime started after Wi-Fi connection");
            } else {
                ESP_LOGW(TAG, "MQTT runtime start failed: 0x%02x", ret);
            }
        }

        return;
    }

    if (g_mqtt_runtime_started) {
        esp_err_t ret = mqtt_client_stop();
        if (ret == ESP_OK) {
            g_mqtt_runtime_started = false;
            app_refresh_mqtt_status();
            ESP_LOGI(TAG, "MQTT runtime stopped because Wi-Fi is not connected");
        } else {
            ESP_LOGW(TAG, "MQTT runtime stop failed: 0x%02x", ret);
        }
    }
}

static void app_log_mqtt_help(void)
{
    ESP_LOGI(TAG, "Serial MQTT commands:");
    ESP_LOGI(TAG, "  MQTT SHOW");
    ESP_LOGI(TAG, "  MQTT RESET");
}

static void app_log_current_mqtt_status(void)
{
    app_refresh_mqtt_status();
    ESP_LOGI(TAG,
             "MQTT state=%s published=%lu failed=%lu broker=%s",
             app_get_mqtt_state_text(),
             (unsigned long)g_mqtt_status.messages_published,
             (unsigned long)g_mqtt_status.messages_failed,
             g_mqtt_status.broker_url[0] != '\0' ? g_mqtt_status.broker_url : "-");
}

static void app_read_ads1015_raw(void)
{
    ads1015_result_t voltage_result = {0};
    ads1015_result_t current_result = {0};
    ads1015_channel_t voltage_channel = (ads1015_channel_t)ADS1015_CHANNEL_VOLTAGE;
    ads1015_channel_t current_channel = (ads1015_channel_t)ADS1015_CHANNEL_CURRENT;
    ads1015_gain_t voltage_gain = (ads1015_gain_t)ADS1015_GAIN_VOLTAGE;
    ads1015_gain_t current_gain = (ads1015_gain_t)ADS1015_GAIN_CURRENT;

    g_ads_diag.voltage_status = ads1015_hal_read_channel(voltage_channel,
                                                         voltage_gain,
                                                         &voltage_result);
    g_ads_diag.voltage_valid = (g_ads_diag.voltage_status == ESP_OK) && voltage_result.data_ready;
    if (g_ads_diag.voltage_valid) {
        g_ads_diag.voltage_raw = voltage_result.raw_value;
        g_ads_diag.voltage_display_raw = (int16_t)(
            g_ads_diag.voltage_display_raw +
            ((g_ads_diag.voltage_raw - g_ads_diag.voltage_display_raw) >> UI_FILTER_SHIFT));
    }

    g_ads_diag.current_status = ads1015_hal_read_channel(current_channel,
                                                         current_gain,
                                                         &current_result);
    g_ads_diag.current_valid = (g_ads_diag.current_status == ESP_OK) && current_result.data_ready;
    if (g_ads_diag.current_valid) {
        g_ads_diag.current_raw = current_result.raw_value;
        g_ads_diag.current_display_raw = (int16_t)(
            g_ads_diag.current_display_raw +
            ((g_ads_diag.current_raw - g_ads_diag.current_display_raw) >> UI_FILTER_SHIFT));
    }

}

static void app_log_ads1015_status(TickType_t *last_log_tick)
{
    TickType_t current_tick;
    static bool s_last_ads_ok = false;
    bool ads_ok;

    if (last_log_tick == NULL) {
        return;
    }

    ads_ok = g_ads_diag.voltage_valid && g_ads_diag.current_valid;
    current_tick = xTaskGetTickCount();
    if ((ads_ok == s_last_ads_ok) &&
        ((current_tick - *last_log_tick) < pdMS_TO_TICKS(RAW_LOG_PERIOD_MS))) {
        return;
    }

    *last_log_tick = current_tick;
    s_last_ads_ok = ads_ok;

    if (ads_ok) {
        ESP_LOGI(TAG, "ADS raw samples: V=%d I=%d",
                 (int)g_ads_diag.voltage_raw,
                 (int)g_ads_diag.current_raw);
        return;
    }

    ESP_LOGW(TAG, "ADS raw read status: V=0x%02x I=0x%02x",
             g_ads_diag.voltage_status,
             g_ads_diag.current_status);
}

static void app_update_measurement_health(void)
{
    TickType_t current_tick = xTaskGetTickCount();

    if (g_ads_diag.voltage_valid && g_ads_diag.current_valid) {
        return;
    }

    if (!g_last_measurement_valid) {
        return;
    }

    if ((current_tick - g_last_measurement_valid_tick) >= pdMS_TO_TICKS(MEASUREMENT_STALE_TIMEOUT_MS)) {
        g_last_measurement_valid = false;
        ESP_LOGW(TAG, "Measurement marked invalid due to stale ADS1015 data");
    }
}

static void app_render_diagnostic_screen(void)
{
    if (g_menu_active) {
        app_render_main_menu_screen();
    } else {
        switch (g_active_screen) {
            case APP_SCREEN_PANEL:
                app_render_panel_screen();
                break;
            case APP_SCREEN_SUMMARY:
                app_render_summary_screen();
                break;
            case APP_SCREEN_STATE:
                app_render_state_screen();
                break;
            case APP_SCREEN_RAW:
            default:
                app_render_raw_screen();
                break;
        }
    }

    if (oled_flush() != ESP_OK) {
        ESP_LOGW(TAG, "OLED flush failed");
    }
}

static void app_render_main_menu_screen(void)
{
    char line_text[OLED_STATUS_TEXT_LENGTH];
    static const char *const menu_lines[APP_MENU_COUNT] = {
        "1 PAINEL",
        "2 SERVICOS",
        "3 METRICAS",
        "4 SENSOR RAW"
    };

    if (!g_oled_ready) {
        return;
    }

    if (oled_clear() != ESP_OK) {
        ESP_LOGW(TAG, "OLED clear failed");
        return;
    }

    app_render_header("MENU");

    for (uint8_t index = 0U; index < (uint8_t)APP_MENU_COUNT; ++index) {
        bool selected = (index == (uint8_t)g_menu_selection);

        (void)snprintf(line_text, sizeof(line_text), "%s", menu_lines[index]);
        (void)oled_draw_menu_marker((uint8_t)(1U + index), 0U, selected);
        (void)oled_draw_text((uint8_t)(1U + index), 10U, line_text);
    }

    (void)snprintf(line_text, sizeof(line_text), "ATUAL:%s", app_get_screen_title(g_active_screen));
    (void)oled_draw_text(6U, 0U, line_text);
    app_render_footer("UP/DN MOVE SEL OK");
}

static void app_render_horizontal_rule(uint8_t page)
{
    (void)oled_draw_pattern_span(page, 0U, OLED_WIDTH_PIXELS, 0x18U);
}

static void app_render_centered_text(uint8_t page,
                                     uint8_t column,
                                     uint8_t width,
                                     const char *text)
{
    size_t text_length;
    size_t text_width;
    uint8_t centered_column = column;

    if ((text == NULL) || (width == 0U)) {
        return;
    }

    text_length = strlen(text);
    if (text_length == 0U) {
        return;
    }

    text_width = text_length * (OLED_GLYPH_WIDTH + OLED_GLYPH_SPACING);
    if (text_width > width) {
        text_width = width;
    } else {
        centered_column = (uint8_t)(column + ((width - text_width) / 2U));
    }

    (void)oled_draw_text(page, centered_column, text);
}

static app_display_snapshot_t app_capture_display_snapshot(void)
{
    app_display_snapshot_t snapshot = {
        .active_screen = g_active_screen,
        .menu_active = g_menu_active,
        .menu_selection = g_menu_selection,
        .measurement_valid = g_last_measurement_valid,
        .voltage_tenths = (int)(g_last_measurement.voltage_rms * 10.0f),
        .current_hundredths = (int)(g_last_measurement.current_rms * 100.0f),
        .power_tenths = (int)(g_last_measurement.power_real * 10.0f),
        .pf_hundredths = (int)(g_last_measurement.power_factor * 10000.0f),
        .wifi_state = (uint8_t)g_wifi_status.state,
        .mqtt_state = (uint8_t)g_mqtt_status.state,
        .ap_active = g_wifi_status.ap_active,
        .ads_voltage_valid = g_ads_diag.voltage_valid,
        .ads_current_valid = g_ads_diag.current_valid,
        .voltage_raw = (int)g_ads_diag.voltage_display_raw,
        .current_raw = (int)g_ads_diag.current_display_raw,
        .messages_published = (unsigned long)g_mqtt_status.messages_published
    };

    return snapshot;
}

static bool app_display_snapshot_changed(const app_display_snapshot_t *current,
                                         const app_display_snapshot_t *previous)
{
    if ((current == NULL) || (previous == NULL)) {
        return true;
    }

    return memcmp(current, previous, sizeof(*current)) != 0;
}

static void app_format_panel_metric(char *buffer,
                                    size_t buffer_length,
                                    const char *unit,
                                    float value,
                                    uint8_t decimals)
{
    char format[16];

    if ((buffer == NULL) || (buffer_length == 0U) || (unit == NULL)) {
        return;
    }

    (void)snprintf(format, sizeof(format), "%%.%uf%%s", (unsigned int)decimals);
    (void)snprintf(buffer, buffer_length, format, (double)value, unit);
}

static void app_render_panel_screen(void)
{
    char left_text[OLED_STATUS_TEXT_LENGTH];
    char right_text[OLED_STATUS_TEXT_LENGTH];

    if (!g_oled_ready) {
        return;
    }

    if (oled_clear() != ESP_OK) {
        ESP_LOGW(TAG, "OLED clear failed");
        return;
    }

    app_render_header("PANEL");

    if (g_last_measurement_valid) {
        app_render_centered_text(1U, 0U, 64U, "TENSAO");
        app_render_centered_text(1U, 64U, 64U, "POTENCIA");

        app_format_panel_metric(left_text,
                                sizeof(left_text),
                                "V",
                                g_last_measurement.voltage_rms,
                                1U);
        app_format_panel_metric(right_text,
                                sizeof(right_text),
                                "W",
                                g_last_measurement.power_real,
                                1U);
        app_render_centered_text(2U, 0U, 64U, left_text);
        app_render_centered_text(2U, 64U, 64U, right_text);
        (void)oled_draw_pattern_span(1U, 63U, 2U, 0x7EU);
        (void)oled_draw_pattern_span(2U, 63U, 2U, 0x7EU);
        app_render_horizontal_rule(3U);

        app_render_centered_text(4U, 0U, 64U, "CORRENTE");
        app_render_centered_text(4U, 64U, 64U, "F.P");

        app_format_panel_metric(right_text,
                                sizeof(right_text),
                                "%",
                                g_last_measurement.power_factor * 100.0f,
                                2U);
        app_format_panel_metric(left_text,
                                sizeof(left_text),
                                "A",
                                g_last_measurement.current_rms,
                                2U);
        app_render_centered_text(5U, 0U, 64U, left_text);
        app_render_centered_text(5U, 64U, 64U, right_text);
        (void)oled_draw_pattern_span(4U, 63U, 2U, 0x7EU);
        (void)oled_draw_pattern_span(5U, 63U, 2U, 0x7EU);
    } else {
        (void)oled_draw_text(2U, 26U, "NO DATA");
        (void)oled_draw_text(4U, 14U, "SELECT MENU");
        (void)oled_draw_text(5U, 8U, "WAIT FOR RMS");
    }

    app_render_footer("UP-DN SEL:NEXT");
}

static void app_render_raw_screen(void)
{
    char voltage_text[OLED_STATUS_TEXT_LENGTH];
    char current_text[OLED_STATUS_TEXT_LENGTH];
    char line_text[OLED_STATUS_TEXT_LENGTH];

    if (!g_oled_ready) {
        return;
    }

    if (oled_clear() != ESP_OK) {
        ESP_LOGW(TAG, "OLED clear failed");
        return;
    }

    app_render_header("RAW");
    (void)snprintf(line_text, sizeof(line_text), "NET:%s AP:%d",
                   app_get_wifi_state_text(),
                   g_wifi_status.ap_active ? 1 : 0);
    (void)oled_draw_text(1U, 0U, line_text);
    (void)snprintf(line_text, sizeof(line_text), "ADS:%s I2C:OK", app_get_ads_state_text());
    (void)oled_draw_text(2U, 0U, line_text);
    (void)snprintf(line_text, sizeof(line_text), "ROT:%s", app_get_rotation_state_text());
    (void)oled_draw_text(3U, 0U, line_text);

    app_format_raw_line(voltage_text,
                        sizeof(voltage_text),
                        'V',
                        g_ads_diag.voltage_valid,
                        g_ads_diag.voltage_display_raw,
                        g_ads_diag.voltage_status);
    app_format_raw_line(current_text,
                        sizeof(current_text),
                        'I',
                        g_ads_diag.current_valid,
                        g_ads_diag.current_display_raw,
                        g_ads_diag.current_status);

    (void)oled_draw_text(4U, 0U, voltage_text);
    (void)oled_draw_text(5U, 0U, current_text);
    (void)snprintf(line_text, sizeof(line_text), "RMS:%s", app_get_measurement_state_text());
    (void)oled_draw_text(6U, 0U, line_text);

    app_render_footer("UP-DN SEL:NEXT");
}

static void app_render_summary_screen(void)
{
    char voltage_text[OLED_STATUS_TEXT_LENGTH];
    char current_text[OLED_STATUS_TEXT_LENGTH];
    char power_text[OLED_STATUS_TEXT_LENGTH];
    char pf_text[OLED_STATUS_TEXT_LENGTH];
    char status_text[OLED_STATUS_TEXT_LENGTH];

    if (!g_oled_ready) {
        return;
    }

    if (oled_clear() != ESP_OK) {
        ESP_LOGW(TAG, "OLED clear failed");
        return;
    }

    app_render_header("RMS");
    (void)snprintf(status_text, sizeof(status_text), "NET:%s", app_get_wifi_state_text());
    (void)oled_draw_text(1U, 0U, status_text);
    (void)snprintf(status_text, sizeof(status_text), "MQTT:%s", app_get_mqtt_state_text());
    (void)oled_draw_text(2U, 0U, status_text);
    (void)snprintf(status_text, sizeof(status_text), "ROT:%s", app_get_rotation_state_text());
    (void)oled_draw_text(3U, 0U, status_text);

    if (g_last_measurement_valid) {
        app_format_measurement_line(voltage_text,
                                    sizeof(voltage_text),
                                    'V',
                                    g_last_measurement.voltage_rms,
                                    1U);
        app_format_measurement_line(current_text,
                                    sizeof(current_text),
                                    'I',
                                    g_last_measurement.current_rms,
                                    2U);
        app_format_measurement_line(power_text,
                                    sizeof(power_text),
                                    'W',
                                    g_last_measurement.power_real,
                                    1U);
        app_format_measurement_line(pf_text,
                                    sizeof(pf_text),
                                    'F',
                                    g_last_measurement.power_factor,
                                    2U);

        (void)oled_draw_text(4U, 0U, voltage_text);
        (void)oled_draw_text(5U, 0U, current_text);
        (void)oled_draw_text(6U, 0U, power_text);
    } else {
        (void)snprintf(status_text, sizeof(status_text), "RMS:%s", app_get_measurement_state_text());
        (void)oled_draw_text(4U, 0U, status_text);
        (void)snprintf(status_text, sizeof(status_text), "ADS:%s", app_get_ads_state_text());
        (void)oled_draw_text(5U, 0U, status_text);
        (void)snprintf(status_text, sizeof(status_text), "IP:%s", g_wifi_status.ip_address);
        (void)oled_draw_text(6U, 0U, status_text);
    }

    if (g_last_measurement_valid) {
        app_render_footer(pf_text);
    } else {
        app_render_footer("RMS HOLD/ON");
    }
}

static void app_render_state_screen(void)
{
    char line_text[OLED_STATUS_TEXT_LENGTH];
    char footer_text[OLED_STATUS_TEXT_LENGTH];

    if (!g_oled_ready) {
        return;
    }

    if (oled_clear() != ESP_OK) {
        ESP_LOGW(TAG, "OLED clear failed");
        return;
    }

    app_render_header("STATE");

    (void)snprintf(line_text, sizeof(line_text), "NET:%s AP:%d",
                   app_get_wifi_state_text(),
                   g_wifi_status.ap_active ? 1 : 0);
    (void)oled_draw_text(1U, 0U, line_text);
    (void)snprintf(line_text, sizeof(line_text), "MQTT:%s", app_get_mqtt_state_text());
    (void)oled_draw_text(2U, 0U, line_text);
    (void)snprintf(line_text, sizeof(line_text), "ADS:%s", app_get_ads_state_text());
    (void)oled_draw_text(3U, 0U, line_text);
    (void)snprintf(line_text, sizeof(line_text), "RMS:%s", app_get_measurement_state_text());
    (void)oled_draw_text(4U, 0U, line_text);
    (void)snprintf(line_text, sizeof(line_text), "IP:%s", g_wifi_status.ip_address);
    (void)oled_draw_text(5U, 0U, line_text);
    (void)snprintf(line_text, sizeof(line_text), "ROT:%s", app_get_rotation_state_text());
    (void)oled_draw_text(6U, 0U, line_text);
    (void)snprintf(footer_text, sizeof(footer_text), "PUB:%lu",
                   (unsigned long)g_mqtt_status.messages_published);
    app_render_footer(footer_text);
}

/**
 * @brief Initialize the Energy Analyzer application
 */
esp_err_t energy_analyzer_app_init(void)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "Initializing Energy Analyzer application...");

    ret = i2c_hal_init(CONFIG_I2C_PORT,
                       CONFIG_I2C_SDA_PIN,
                       CONFIG_I2C_SCL_PIN,
                       CONFIG_I2C_FREQUENCY_HZ);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C HAL initialization failed: 0x%02x", ret);
        return ret;
    }

    ret = oled_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OLED initialization failed: 0x%02x", ret);
        return ret;
    }
    g_oled_ready = true;

    ret = app_buttons_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Button initialization failed: 0x%02x", ret);
        return ret;
    }

    ret = ads1015_hal_init();
    if (ret == ESP_OK) {
        g_ads1015_hal_ready = true;
    } else {
        g_ads1015_hal_ready = false;
        ESP_LOGW(TAG, "ADS1015 HAL initialization failed: 0x%02x", ret);
    }

    ret = measurement_service_init();
    if (ret == ESP_OK) {
        g_measurement_ready = true;
    } else {
        g_measurement_ready = false;
        ESP_LOGW(TAG, "Measurement service initialization failed: 0x%02x", ret);
    }

    ret = wifi_service_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi service initialization failed: 0x%02x", ret);
    } else {
        app_refresh_wifi_status();
    }

    ret = mqtt_client_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MQTT client initialization failed: 0x%02x", ret);
    }

    app_render_diagnostic_screen();
    app_log_calibration_help();
    app_log_current_calibration();
    app_log_wifi_help();
    app_log_current_wifi_status();
    app_log_mqtt_help();
    app_log_current_mqtt_status();

    ESP_LOGI(TAG, "Energy Analyzer application initialized successfully");
    return ESP_OK;
}

/**
 * @brief Start the main application tasks
 */
esp_err_t energy_analyzer_app_start(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Starting Energy Analyzer tasks...");
    ESP_LOGI(TAG, "OLED diagnostic UI active");

    ret = wifi_service_start();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi service start failed: 0x%02x", ret);
    } else {
        app_refresh_wifi_status();
        app_log_current_wifi_status();
    }

    if (app_wifi_ready_for_mqtt()) {
        ret = mqtt_client_start(NULL);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "MQTT client start failed: 0x%02x", ret);
        } else {
            g_mqtt_runtime_started = true;
            ESP_LOGI(TAG, "MQTT client started");
        }
    } else {
        ESP_LOGI(TAG, "MQTT start deferred until Wi-Fi is connected");
    }

    return ESP_OK;
}

/**
 * @brief Main application loop
 */
void energy_analyzer_app_run(void)
{
    TickType_t last_raw_log_tick = 0U;
    app_display_snapshot_t last_display_snapshot = {0};
    bool display_snapshot_valid = false;

    ESP_LOGI(TAG, "Starting Energy Analyzer main loop...");

    while (1) {
        // Serial calibration is intentionally out of the active production path.
        app_process_buttons();
        app_refresh_wifi_status();
        app_refresh_mqtt_status();
        app_manage_mqtt_runtime();
        app_refresh_mqtt_status();

        if (g_ads1015_hal_ready) {
            app_read_ads1015_raw();
            app_log_ads1015_status(&last_raw_log_tick);
        }
        app_update_measurement_health();

        if (g_ads_diag.voltage_valid && g_ads_diag.current_valid && g_measurement_ready) {
            uint16_t voltage_raw;
            uint16_t current_raw;
            measurement_result_t result;
            esp_err_t ret;

            voltage_raw = (uint16_t)((g_ads_diag.voltage_raw + 2048) * 4095 / 4096);
            current_raw = (uint16_t)((g_ads_diag.current_raw + 2048) * 4095 / 4096);

            ret = measurement_service_process_samples(voltage_raw, current_raw, &result);
            if (ret == ESP_OK) {
                g_last_measurement = result;
                g_last_measurement_valid = true;
                g_last_measurement_valid_tick = xTaskGetTickCount();
                if ((xTaskGetTickCount() - g_last_rms_log_tick) >= pdMS_TO_TICKS(RMS_LOG_PERIOD_MS)) {
                    g_last_rms_log_tick = xTaskGetTickCount();
                    ESP_LOGI(TAG, "RMS: U=%.2fV, I=%.3fA, P=%.1fW, PF=%.3f",
                             result.voltage_rms,
                             result.current_rms,
                             result.power_real,
                             result.power_factor);
                }

                // Publish telemetry if enough time has passed
                if (((xTaskGetTickCount() - g_last_mqtt_publish_tick) >= pdMS_TO_TICKS(MQTT_PUBLISH_PERIOD_MS)) &&
                    mqtt_client_is_connected()) {
                    g_last_mqtt_publish_tick = xTaskGetTickCount();

                    mqtt_telemetry_t telemetry = {
                        .voltage_rms = result.voltage_rms,
                        .current_rms = result.current_rms,
                        .power_real = result.power_real,
                        .power_factor = result.power_factor,
                        .timestamp_ms = result.timestamp_ms
                    };

                    esp_err_t mqtt_ret = mqtt_client_publish_telemetry(&telemetry);
                    if (mqtt_ret != ESP_OK) {
                        ESP_LOGW(TAG, "Failed to publish MQTT telemetry: 0x%02x", mqtt_ret);
                    }
                }
            }
        }

        if (g_oled_ready) {
            app_display_snapshot_t current_snapshot = app_capture_display_snapshot();

            if (!display_snapshot_valid ||
                app_display_snapshot_changed(&current_snapshot, &last_display_snapshot)) {
                app_render_diagnostic_screen();
                last_display_snapshot = current_snapshot;
                display_snapshot_valid = true;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_PERIOD_MS));
    }
}
