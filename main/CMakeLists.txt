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
    INCLUDE_DIRS 
        "inc"
    PRIV_REQUIRES
        driver
        u8g2
        nvs_flash
        esp_wifi
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE U8G2_USE_LARGE_FONTS=0)
