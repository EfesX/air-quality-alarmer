#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "driver/i2c.h"
#include "esp_log.h"

#include "main.h"

#define AHT21_DEV_ADDR  0x38
#define ENS160_DEV_ADDR 0x53
#define BMP280_DEV_ADDR 0x76

#define INTERVAL_MEASURMENT_MS 1000

#define AHT21_TEMPERATURE_OFFSET -4.0f
#define AHT21_HUMIDITY_GAIN       0.85f

#define BMP280_TEMPERATURE_OFFSET -2.0f
#define BMP280_PRESSURE_GAIN       1.0f

typedef struct {
    SemaphoreHandle_t i2c_smphr;
    QueueHandle_t sensors_queue;
} measurment_task_config_t;

void measurment_task(void *arg);
