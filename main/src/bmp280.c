#include "measurment.h"

/* registers */
#define BMP280_REG_CHIP_ID   0xd0
#define BMP280_REG_RESET     0xe0
#define BMP280_REG_CTRL_MEAS 0xf4
#define BMP280_REG_CONFIG    0xf5
#define BMP280_REG_PRESS_MSB 0xf7

typedef struct {
    uint16_t T1;
    int16_t T2;
    int16_t T3;
    uint16_t P1;
    int16_t P2;
    int16_t P3;
    int16_t P4;
    int16_t P5;
    int16_t P6;
    int16_t P7;
    int16_t P8;
    int16_t P9;
    int32_t t_fine;
} calib_data_t;
static calib_data_t calib_data;


static esp_err_t bmp280_read_register(uint8_t reg_addr, uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BMP280_DEV_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BMP280_DEV_ADDR << 1) | I2C_MASTER_READ, true);
    if(len > 1)
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ok = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ok;
}

static esp_err_t bmp280_write_register(uint8_t reg_addr, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BMP280_DEV_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ok = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ok;
}

static void bmp280_read_calibration_data(void)
{
    uint8_t data[24];
    ESP_ERROR_CHECK(bmp280_read_register(0x88, data, sizeof(data)));

    calib_data.T1 = (data[1] << 8) | data[0];
    calib_data.T2 = (data[3] << 8) | data[2];
    calib_data.T3 = (data[5] << 8) | data[4];

    calib_data.P1 = (data[ 7] << 8) | data[ 6];
    calib_data.P2 = (data[ 9] << 8) | data[ 8];
    calib_data.P3 = (data[11] << 8) | data[10];
    calib_data.P4 = (data[13] << 8) | data[12];
    calib_data.P5 = (data[15] << 8) | data[14];
    calib_data.P6 = (data[17] << 8) | data[16];
    calib_data.P7 = (data[19] << 8) | data[18];
    calib_data.P8 = (data[21] << 8) | data[20];
    calib_data.P9 = (data[23] << 8) | data[22];
}

static float bmp280_compensate_temperature(int32_t adc_T)
{
    int32_t var1, var2, T;

    var1 = ((((adc_T >> 3) - ((int32_t)calib_data.T1 << 1))) * ((int32_t)calib_data.T2)) >> 11;
    var2 = ((((adc_T >> 4) - ((int32_t)calib_data.T1)) * ((adc_T >> 4) - ((int32_t)calib_data.T1))) >> 12) * ((int32_t)calib_data.T3) >> 14;

    calib_data.t_fine = var1 + var2;
    T = (calib_data.t_fine * 5 + 128) >> 8;

    return (float)T/100.0f;
}

static float bmp280_compensate_pressure(int32_t adc_P)
{
    int64_t var1, var2, p;

    var1 = ((int64_t)calib_data.t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calib_data.P6;
    var2 = var2 + ((var1 * (int64_t)calib_data.P5) << 17);
    var2 = var2 + (((int64_t)calib_data.P4) << 35);
    var1 = (((var1 * var1 * (int64_t)calib_data.P3) >> 8) + ((var1 * (int64_t)calib_data.P2) << 12));
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t) calib_data.P1) >> 33;

    if(var1 == 0)
        return 0.0f;

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)calib_data.P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)calib_data.P8) * p) >> 19;

    p = ((p + var1 + var2) >> 8) + (((int64_t)calib_data.P7) << 4);

    return (float)p/256.0f;
}

esp_err_t bmp280_init(void)
{
    uint8_t chip_id = 0;
    ESP_ERROR_CHECK(bmp280_read_register(BMP280_REG_CHIP_ID, &chip_id, 1));
    assert(
        chip_id == 0x60 || 
        chip_id == 0x56 || 
        chip_id == 0x57 || 
        chip_id == 0x58
    );

    // reset sensor
    ESP_ERROR_CHECK(bmp280_write_register(BMP280_REG_RESET, 0xb6));
    vTaskDelay(pdMS_TO_TICKS(10));

    bmp280_read_calibration_data();

    // setup work mode
    // normal mode, temp oversampling x16, press oversampling x16
    ESP_ERROR_CHECK(bmp280_write_register(BMP280_REG_CTRL_MEAS, 0xf7)); 
    // standby 0.5ms, filter off
    ESP_ERROR_CHECK(bmp280_write_register(BMP280_REG_CONFIG,    0x00));

    return ESP_OK;
}

esp_err_t bmp280_read(bmp280_data_t *result)
{   
    uint8_t data[6];
    int32_t adc_T, adc_P;

    // read all data
    esp_err_t ok = bmp280_read_register(BMP280_REG_PRESS_MSB, data, 6);

    adc_P = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    adc_T = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);

    result->temperature = bmp280_compensate_temperature(adc_T);
    result->pressure = bmp280_compensate_pressure(adc_P) * 0.00750062f;

    result->temperature += BMP280_TEMPERATURE_OFFSET;
    result->pressure    *= BMP280_PRESSURE_GAIN;

    return ok;
}
