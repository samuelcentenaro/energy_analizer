idf_component_register(
    SRCS "wifi_service.c" "mqtt_client.c"
    INCLUDE_DIRS "include"
    REQUIRES board_hal config services nvs_flash esp_wifi esp_event esp_netif esp_http_server mqtt
    PRIV_REQUIRES freertos
)
