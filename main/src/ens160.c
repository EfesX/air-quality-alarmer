#include "measurment.h"

#define ENS160_ID 0x0160


static inline uint16_t ens160_read_id(void)
{
    uint8_t wdata = 0x00;
    uint16_t ens160_id = 0x0000;
    esp_err_t ok = i2c_master_write_read_device(I2C_MASTER_NUM, ENS160_DEV_ADDR, 
        &wdata, 1, (uint8_t*)&ens160_id, 2, I2C_MASTER_TIMEOUT
    );
    ESP_ERROR_CHECK(ok);

    return ens160_id;
}

esp_err_t ens160_init(void)
{
    esp_err_t ok = ESP_OK;

    assert(ens160_read_id() == ENS160_ID);

    // set ens160 opmode == 0x02 (standard gas sensing mode)
    uint8_t wdata[2] = {0x10, 0x02};
    ok = i2c_master_write_to_device(I2C_MASTER_NUM, 
        ENS160_DEV_ADDR, wdata, 2, I2C_MASTER_TIMEOUT
    );
    
    return ok;
}

esp_err_t ens160_compensate(aht21_data_t *data)
{
    float temp = data->temperature + 273.15;
    uint16_t temperature_code = ((uint16_t) temp) * 64UL;

    temp = data->humidity * 512.0;
    uint16_t humidity_code = (uint16_t) temp;

    uint8_t wdata[5];
    wdata[0] = 0x13;
    wdata[1] = (uint8_t)temperature_code;
    wdata[2] = (uint8_t)(temperature_code >> 8);
    wdata[3] = (uint8_t)humidity_code;
    wdata[4] = (uint8_t)(humidity_code >> 8);

    return i2c_master_write_to_device(I2C_MASTER_NUM, 
        ENS160_DEV_ADDR, wdata, sizeof(wdata), I2C_MASTER_TIMEOUT
    );
}

ens160_data_t ens160_read(void)
{
    ens160_data_t readen;

    uint8_t wdata = 0x20;
    uint8_t rdata[6] = {0};
    esp_err_t ok = ESP_OK;
    ok = i2c_master_write_read_device(I2C_MASTER_NUM, ENS160_DEV_ADDR, 
        &wdata, 1, rdata, sizeof(rdata), I2C_MASTER_TIMEOUT
    );
    ESP_ERROR_CHECK(ok);
    readen.status = rdata[0];
    readen.aqi = rdata[1] & 0x07;
    readen.tvoc = (((uint16_t)rdata[3]) << 8) | ((uint16_t)rdata[2]);
    readen.eco2 = (((uint16_t)rdata[5]) << 8) | ((uint16_t)rdata[4]);

    return readen;
}

esp_err_t ens160_reset(void)
{
    esp_err_t ok = ESP_OK;

    { // set ens160 opmode == 0xf0 (reset state)
        uint8_t wdata[2] = {0x10, 0xf0};
        ok = i2c_master_write_to_device(I2C_MASTER_NUM, ENS160_DEV_ADDR, 
            wdata, 2, I2C_MASTER_TIMEOUT
        );
        ESP_ERROR_CHECK(ok);
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    { // set ens160 opmode == 0x02 (standard gas sensing mode)
        uint8_t wdata[2] = {0x10, 0x02};
        ok = i2c_master_write_to_device(I2C_MASTER_NUM, ENS160_DEV_ADDR, 
            wdata, 2, I2C_MASTER_TIMEOUT
        );
        ESP_ERROR_CHECK(ok);
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    return ok;
}