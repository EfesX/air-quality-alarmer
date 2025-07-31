#pragma once

#include <stdint.h>
#include <stdbool.h>

#define I2C_MASTER_NUM              I2C_NUM_0  
#define I2C_MASTER_TIMEOUT          (1000 / portTICK_PERIOD_MS)

#define DISPLAY_LOGO_TIME_MS        3000

#define LOG_SENSORS_ENABLE 0

#define MIN(a,b) (a < b ? a : b)

typedef struct {
    uint8_t status;
    float temperature;
    float humidity;
    bool crc_ok;
} aht21_data_t;

typedef struct {
    float temperature;
    float pressure;
} bmp280_data_t;

typedef struct {
    uint8_t status;
    /**
     * UBA Air Quality Index
    */
    uint8_t aqi;

    /**
     * Total Volatile Organic Compounds
     * unit: ppb
     * resolution: 1
    */
    uint16_t tvoc;

    /**
     * Equivalent CO2
     * resolution : 1
     * unit: ppm
     * eCO2 ranges:
     * 400 - 600   : excellent.         Target
     * 600 - 800   : good.              Average
     * 800 - 1000  : fair.              Optional ventilation
     * 1000 - 1500 : poor.              Contaminated indoor air / Ventilation recommended
     * >1500       : bad.               Heavily contaminated indoor air / Ventilation required
    */
    uint16_t eco2;
} ens160_data_t;

typedef struct {
    aht21_data_t aht21;
    bmp280_data_t bmp280;
    ens160_data_t ens160;
} sensors_data_t;


void reboot_task(void* arg);
