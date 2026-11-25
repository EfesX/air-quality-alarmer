#pragma once

#include <freertos/FreeRTOS.h>

#define SSD1306_DEV_ADDR 0x3c

typedef struct {
    SemaphoreHandle_t i2c_smphr;
    QueueHandle_t queue;
} display_task_config_t;

void display_task(void* arg);
