idf_component_register(
    SRCS "board_hal.c" "ads1015_hal.c" "i2c_hal.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_gpio config
)
