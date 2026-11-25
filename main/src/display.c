#include "main.h"
#include "display.h"

#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"
#include "esp_log.h"

#include "u8g2.h"
#include "u8g2_esp32_hal.h"

static SemaphoreHandle_t i2c_smphr = NULL;

uint8_t cb_i2c_display(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) 
{
    static i2c_cmd_handle_t handle_i2c;

    switch (msg)
    {
    case U8X8_MSG_BYTE_SEND:{
        uint8_t* data_ptr = (uint8_t*)arg_ptr;
        while (arg_int > 0) 
        {
            ESP_ERROR_CHECK(i2c_master_write_byte(handle_i2c, *data_ptr, ACK_CHECK_EN));
            data_ptr++;
            arg_int--;
        }
        break;
    }

    case U8X8_MSG_BYTE_START_TRANSFER: 
    {
        uint8_t i2c_address = u8x8_GetI2CAddress(u8x8);
        handle_i2c = i2c_cmd_link_create();
        ESP_ERROR_CHECK(i2c_master_start(handle_i2c));
        ESP_ERROR_CHECK(i2c_master_write_byte(
            handle_i2c, i2c_address | I2C_MASTER_WRITE, ACK_CHECK_EN));
        break;
    }

    case U8X8_MSG_BYTE_END_TRANSFER: 
    {
        if(i2c_smphr == NULL)
            break;

        ESP_ERROR_CHECK(i2c_master_stop(handle_i2c));

        if(xSemaphoreTake(i2c_smphr, pdMS_TO_TICKS(50)) == pdFALSE)
            break;

        ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, handle_i2c, I2C_MASTER_TIMEOUT));
        xSemaphoreGive(i2c_smphr);

        i2c_cmd_link_delete(handle_i2c);
        break;
    }
    
    default:
        break;
    }
    return 0;
}

void display_task(void* arg)
{

    const display_task_config_t *config = (display_task_config_t *) arg;
    i2c_smphr = config->i2c_smphr;

    
    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal_init(u8g2_esp32_hal);

    u8g2_t u8g2;

    u8g2_Setup_ssd1306_i2c_128x32_univision_f(
        &u8g2, 
        U8G2_R0,
        cb_i2c_display,
        u8g2_esp32_gpio_and_delay_cb
    );
    u8x8_SetI2CAddress(&u8g2.u8x8, SSD1306_DEV_ADDR << 1);
    
    u8g2_InitDisplay(&u8g2);
    
    u8g2_SetPowerSave(&u8g2, 0);
    
    u8g2_SetFont(&u8g2, u8g2_font_luBS24_tr);
    u8g2_ClearBuffer(&u8g2);
    u8g2_DrawStr(&u8g2, 2, 31, "EFESX");
    u8g2_SendBuffer(&u8g2);

    u8g2_SetFont(&u8g2, u8g2_font_04b_03b_tr);

    vTaskDelay(pdMS_TO_TICKS(DISPLAY_LOGO_TIME_MS));
    
    sensors_data_t sdata;
    char str[256];

    while(1)
    {
        xQueueReceive(config->queue, &sdata, portMAX_DELAY);

        u8g2_ClearBuffer(&u8g2);

        snprintf(str, sizeof(str), "%.2f °C  %.2f %%   %.0f mmhg", sdata.bmp280.temperature, sdata.aht21.humidity, sdata.bmp280.pressure);
        u8g2_DrawStr(&u8g2, 2, 7, str);

        snprintf(str, sizeof(str), "AQI   : %d", sdata.ens160.aqi);
        u8g2_DrawStr(&u8g2, 2, 15, str);

        snprintf(str, sizeof(str), "TVOC  : %d ppb", sdata.ens160.tvoc);
        u8g2_DrawStr(&u8g2, 2, 23, str);

        snprintf(str, sizeof(str), "ECO2  : %d ppm", sdata.ens160.eco2);
        u8g2_DrawStr(&u8g2, 2, 31, str);

        u8g2_SendBuffer(&u8g2);
    }
}
