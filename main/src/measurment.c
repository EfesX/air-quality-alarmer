#include "measurment.h"

esp_err_t aht21_init(void);
esp_err_t aht21_reset(void);
esp_err_t aht21_read_data(aht21_data_t *result);

esp_err_t ens160_init(void);
esp_err_t ens160_compensate(aht21_data_t *data);
ens160_data_t ens160_read(void);
esp_err_t ens160_reset(void);

esp_err_t bmp280_init(void);
esp_err_t bmp280_read(bmp280_data_t *result);

void measurment_task(void *arg)
{
    const measurment_task_config_t *config = 
        (measurment_task_config_t*) arg;
    sensors_data_t sensors_data;

    vTaskDelay(pdMS_TO_TICKS(100));

    xSemaphoreTake(config->i2c_smphr, portMAX_DELAY);
    aht21_reset();
    ens160_reset();
    vTaskDelay(pdMS_TO_TICKS(250));
    aht21_init();
    ens160_init();
    bmp280_init();
    xSemaphoreGive(config->i2c_smphr);
    
    while(true)
    {
        vTaskDelay(pdMS_TO_TICKS(INTERVAL_MEASURMENT_MS));

        xSemaphoreTake(config->i2c_smphr, portMAX_DELAY);
        
        ESP_ERROR_CHECK(aht21_read_data(&sensors_data.aht21));    
        if(sensors_data.aht21.crc_ok == false)
            continue;

        ESP_ERROR_CHECK(bmp280_read(&sensors_data.bmp280));

        aht21_data_t forcomp = {
            .temperature = sensors_data.bmp280.temperature,
            .humidity = sensors_data.aht21.humidity
        };

        ESP_ERROR_CHECK(ens160_compensate(&forcomp));
        vTaskDelay(pdMS_TO_TICKS(50));

        sensors_data.ens160 = ens160_read();

        xSemaphoreGive(config->i2c_smphr);

        if((sensors_data.ens160.status & 0x02) == 0x00)
            continue;

        xQueueSend(config->sensors_queue, &sensors_data, pdMS_TO_TICKS(50));   
    }
}

