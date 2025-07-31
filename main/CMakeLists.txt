idf_component_register(
    SRCS 
        "src/main.c"
        "src/measurment.c"
        "src/display.c"
        "src/u8g2_esp32_hal.c"
        "src/aht21.c"
        "src/ens160.c"
        "src/bmp280.c"
        "src/wifi.c"
        "src/web.c"
        "src/creds.c"
        "src/http_handler_root.c"
        "src/http_handler_save.c"
        "src/http_update_firmware.c"
    INCLUDE_DIRS 
        "inc"
        "../build"
    PRIV_REQUIRES
        driver
        u8g2
        nvs_flash
        esp_wifi
        esp_http_server
        app_update
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE U8G2_USE_LARGE_FONTS=0)
