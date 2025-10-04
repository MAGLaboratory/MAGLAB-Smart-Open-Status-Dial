idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        board
        input
        timer
        ui
        services
)
