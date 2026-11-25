#include "measurment.h"

#define AHT21_CMD_STARTUP     0x71
#define AHT21_CMD_INIT        0xBE
#define AHT21_CMD_TRIGGER     0xAC
#define AHT21_CMD_SOFTRESET   0xBA

esp_err_t aht21_reset(void)
{   
    uint8_t data = AHT21_CMD_SOFTRESET;
    esp_err_t ok = i2c_master_write_to_device(
        I2C_MASTER_NUM, AHT21_DEV_ADDR, 
        &data, 1, I2C_MASTER_TIMEOUT
    );
    return ok;
}   

esp_err_t aht21_init(void)
{
    uint8_t data[3] = {AHT21_CMD_STARTUP, 0x08, 0x00};
    esp_err_t ok = i2c_master_write_to_device(I2C_MASTER_NUM, 
        AHT21_DEV_ADDR, data, 1, I2C_MASTER_TIMEOUT
    );
    ESP_ERROR_CHECK(ok);

    ok = i2c_master_read_from_device(I2C_MASTER_NUM, 
        AHT21_DEV_ADDR, data, 1, I2C_MASTER_TIMEOUT
    );
    ESP_ERROR_CHECK(ok);

    if((data[0] & 0x18) == 0x18)
        return ESP_OK;

    data[0] = AHT21_CMD_INIT;

    ok = i2c_master_write_to_device(I2C_MASTER_NUM, 
        AHT21_DEV_ADDR, data, 3, I2C_MASTER_TIMEOUT
    );
    ESP_ERROR_CHECK(ok);

    vTaskDelay(pdMS_TO_TICKS(20));

    return ESP_OK;
}

static inline uint8_t crc_calculate(uint8_t* data)
{
    uint8_t crc = 0xff;
    for(uint8_t i = 0; i < 6; i++){
        crc ^= data[i];
        for(uint8_t j = 0; j < 8; j++){
            if(crc & 0x80) crc = (crc << 1) ^ 0x31;
            else crc <<= 1;
        }
    }
    return crc;
}

esp_err_t aht21_read_data(aht21_data_t *result) 
{
    uint8_t trigger_cmd[3] = {AHT21_CMD_TRIGGER, 0x33, 0x00};
    uint8_t data[7] = {0};

    // starting measurment
    esp_err_t ok = i2c_master_write_to_device(I2C_MASTER_NUM, 
        AHT21_DEV_ADDR, trigger_cmd, 3, I2C_MASTER_TIMEOUT
    );
    ESP_ERROR_CHECK(ok);

    // wait for finish measurment
    vTaskDelay(pdMS_TO_TICKS(100));

    // read data
    ok = i2c_master_read_from_device(I2C_MASTER_NUM, 
        AHT21_DEV_ADDR, data, sizeof(data), I2C_MASTER_TIMEOUT
    );
    ESP_ERROR_CHECK(ok);

    result->status = data[0];
    result->crc_ok = (crc_calculate(data) == data[6]);


    // data converting
    uint32_t raw_humidity = (((uint32_t)data[1]) << 16);
    raw_humidity |= (((uint32_t)data[2]) << 8);
    raw_humidity |= (uint32_t)data[3];
    raw_humidity >>= 4;

    result->humidity = (float) raw_humidity;
    result->humidity *= 100.0;
    result->humidity /= (float)(1 << 20);
    result->humidity *= AHT21_HUMIDITY_GAIN;
    

    uint32_t raw_temp = (((uint32_t)(data[3] & 0x0F)) << 16);
    raw_temp |= (((uint32_t)data[4]) << 8);
    raw_temp |=  ((uint32_t)data[5]);

    result->temperature = (float) raw_temp;
    result->temperature *= 200.0;
    result->temperature /= (float)(1 << 20);
    result->temperature -= 50.0;
    result->temperature += AHT21_TEMPERATURE_OFFSET;

    return ok;
}
