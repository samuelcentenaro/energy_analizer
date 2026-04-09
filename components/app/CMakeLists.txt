idf_component_register(
    SRCS "energy_analyzer_app.c"
    INCLUDE_DIRS "include"
    REQUIRES board_hal services network
    PRIV_REQUIRES rtos
)
