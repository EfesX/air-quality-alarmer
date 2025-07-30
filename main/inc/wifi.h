#pragma once

#include "esp_err.h"

#define WIFI_STA_MAX_RETRY  3
#define WIFI_STA_CONNECTION_TIMEOUT_MS 15000

#define WIFI_AP_SSID        "ESP_AQA"
#define WIFI_AP_PASSWORD    "12344321"
#define WIFI_AP_CHANNEL     5
#define WIFI_AP_MAX_CONN    1

#define WIFI_ENA_PIN 13

void wifi_start(void);
