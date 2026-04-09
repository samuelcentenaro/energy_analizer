#include <esp_log.h>

#include "energy_analyzer_app.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Energy Analyzer starting...");

    ret = energy_analyzer_app_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Energy Analyzer initialization failed: 0x%02x", ret);
        return;
    }

    ret = energy_analyzer_app_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Energy Analyzer start failed: 0x%02x", ret);
        return;
    }

    energy_analyzer_app_run();
}
