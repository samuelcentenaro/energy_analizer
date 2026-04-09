idf_component_register(
    SRCS "measurement_service.c"
    INCLUDE_DIRS "include"
    REQUIRES config utils nvs_flash
    PRIV_REQUIRES freertos
)
